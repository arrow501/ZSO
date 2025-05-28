# pthread Developer Manual: Master-Slave IPC System

## Table of Contents

1. [Overview](#overview)
2. [pthread Concepts Used](#pthread-concepts-used)
3. [Memory and Thread Safety Design](#memory-and-thread-safety-design)
4. [Debug Assertions](#debug-assertions)
5. [Testing Guide](#testing-guide)
6. [Common Pitfalls and Solutions](#common-pitfalls-and-solutions)
7. [Complete pthread/Semaphore Function Reference](#complete-pthreadsemaphore-function-reference)

## Overview

This system implements inter-process communication (IPC) between a master process and multiple slave processes using:

- **Named pipes (FIFOs)** for message passing
- **Shared memory** for statistics
- **pthread mutexes** for synchronization
- **POSIX semaphores** for signaling
- **poll()** for efficient I/O multiplexing

### Architecture

```
┌─────────┐     Named Pipe      ┌─────────┐
│ Slave 0 │ ←─────────────────→ │         │
└─────────┘                     │         │     Shared Memory
                                │ Master  │ ←→ ┌──────────────┐
┌─────────┐     Named Pipe      │         │    │   Statistics │
│ Slave 1 │ ←─────────────────→ │         │    │   + Mutex    │
└─────────┘                     └─────────┘    └──────────────┘
                                      ↑                ↑
                                      │                │
                                   Semaphore    ┌──────────────┐
                                                │ Stats Reader │
                                                └──────────────┘
```

## pthread Concepts Used

### 1. **pthread_mutex_t** - Mutual Exclusion Lock

```c
pthread_mutex_t mutex;  // Protects shared memory stats
```

- **Purpose**: Prevents race conditions when multiple processes access shared data
- **Usage in project**: Protects the statistics structure in shared memory
- **Key functions used**:
  - `pthread_mutex_init(&stats->mutex, &mutex_attr)` - Initialize with process-shared attributes
  - `pthread_mutex_lock(&stats->mutex)` - Acquire exclusive access (blocks until available)
  - `pthread_mutex_unlock(&stats->mutex)` - Release exclusive access
  - `pthread_mutex_destroy(&stats->mutex)` - Clean up mutex before unmapping memory

**Critical Usage Pattern**:

```c
pthread_mutex_lock(&stats->mutex);     // Always acquire lock first
stats->messages_sent[id]++;             // Modify shared data safely
stats->active_slaves[id] = 1;           // Multiple operations in critical section
pthread_mutex_unlock(&stats->mutex);   // Always release lock
```

### 2. **pthread_mutexattr_t** - Mutex Attributes Configuration

```c
pthread_mutexattr_t mutex_attr;
pthread_mutexattr_init(&mutex_attr);
pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
```

- **Purpose**: Configure mutex behavior for inter-process synchronization
- **Functions used**:
  - `pthread_mutexattr_init(&mutex_attr)` - Initialize attribute object
  - `pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED)` - Enable inter-process sharing
  - `pthread_mutexattr_destroy(&mutex_attr)` - Clean up attribute object

**Why PTHREAD_PROCESS_SHARED is critical**:

- Default mutexes only work within a single process
- `PTHREAD_PROCESS_SHARED` allows mutex to synchronize between different processes
- Without this, mutex operations in shared memory would fail

### 3. **POSIX Named Semaphores** - sem_t

```c
sem_t *stats_ready_sem;  // Named semaphore for signaling
```

- **Purpose**: Synchronization primitive for signaling between processes
- **Usage in project**: Notify external readers when statistics are updated

**Complete function set used**:

- `sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0)` - Create named semaphore (initial value 0)
- `sem_open(SEM_NAME, 0)` - Open existing named semaphore (read-only mode)
- `sem_post(stats_ready_sem)` - Signal/increment semaphore (wake up waiters)
- `sem_timedwait(stats_ready_sem, &timeout)` - Wait with timeout for signal
- `sem_close(stats_ready_sem)` - Close semaphore handle
- `sem_unlink(SEM_NAME)` - Remove named semaphore from system

**Semaphore Flow**:

```c
// Master: Signal stats are ready
sem_post(stats_ready_sem);

// Stats Reader: Wait for stats (with timeout)
struct timespec timeout;
clock_gettime(CLOCK_REALTIME, &timeout);
timeout.tv_sec += 2;  // 2 second timeout
sem_timedwait(stats_ready_sem, &timeout);
```

### 4. **Signal-Safe Atomic Variables**

```c
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t notify_master = 0;
static volatile sig_atomic_t dump_stats = 0;
```

- **Purpose**: Variables that can be safely accessed from signal handlers
- **Type**: `sig_atomic_t` guaranteed to be atomic even without locks
- **Qualifier**: `volatile` prevents compiler optimization that could cause issues
- **Usage**: Signal handlers set these flags, main loops check them

**Why this matters**:

- Signal handlers cannot safely use mutexes or complex operations
- `sig_atomic_t` provides safe communication between signal context and main program
- Prevents race conditions in signal handling

### 5. **Shared Memory with Process-Shared Mutexes**

```c
stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```

- **Purpose**: Memory region accessible by multiple processes
- **Critical**: Must use `PROT_READ | PROT_WRITE` for mutex operations (not just `PROT_READ`)
- **Key**: Process-shared mutex must be embedded in shared memory structure

**Memory Layout Design**:

```c
typedef struct {
    pthread_mutex_t mutex;  // MUST be first for proper alignment
    int messages_sent[MAX_SLAVES];
    int messages_received[MAX_SLAVES];
    int slave_pids[MAX_SLAVES];
    int active_slaves[MAX_SLAVES];
    int magic;  // Integrity verification
} stats_t;
```

**Why mutex is first**: Ensures proper memory alignment across different architectures and prevents potential corruption.

### 6. **Cross-Process Mutex Casting**

```c
// In stats_reader.c - external process accessing shared mutex
pthread_mutex_lock((pthread_mutex_t *)&stats->mutex);
display_stats(stats);
pthread_mutex_unlock((pthread_mutex_t *)&stats->mutex);
```

- **Purpose**: Allow external processes to use mutex in shared memory
- **Critical**: The mutex must have been initialized with `PTHREAD_PROCESS_SHARED`
- **Casting**: Explicit cast tells compiler this is a valid mutex operation

## Memory and Thread Safety Design

### Safety Principles

1. **Lock Ordering**: Always acquire mutex before modifying shared data
2. **Atomic Operations**: Use `sig_atomic_t` for signal-safe variables
3. **Resource Cleanup**: Always pair acquire/release operations
4. **Process-Shared**: Use `PTHREAD_PROCESS_SHARED` for inter-process mutexes

### Critical Sections

#### Master Process - Statistics Updates

```c
// Acquiring lock before updating shared statistics
pthread_mutex_lock(&stats->mutex);
stats->messages_sent[id]++;
stats->active_slaves[id] = 1;
pthread_mutex_unlock(&stats->mutex);
```

#### Stats Reader - Safe Reading

```c
// External process reading statistics safely
pthread_mutex_lock((pthread_mutex_t *)&stats->mutex);
display_stats(stats);
pthread_mutex_unlock((pthread_mutex_t *)&stats->mutex);
```

### Memory Layout Considerations

The statistics structure is carefully designed for optimal mutex placement:

```c
typedef struct {
    pthread_mutex_t mutex;  // MUST be first for alignment
    int messages_sent[MAX_SLAVES];
    int messages_received[MAX_SLAVES];
    int slave_pids[MAX_SLAVES];
    int active_slaves[MAX_SLAVES];
    int magic;  // Integrity verification
} stats_t;
```

**Why mutex is first**: Ensures proper memory alignment across different architectures.

## Debug Assertions

### Thread Safety Validation

```c
#define DEBUG_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", msg); \
            fprintf(stderr, "  at %s:%d in %s\n", __FILE__, __LINE__, __func__); \
            assert(cond); \
        } \
    } while(0)
```

### Key Assertion Points

1. **Mutex State**: Verify mutex is initialized before use
2. **Memory Integrity**: Check magic numbers in shared memory
3. **Process States**: Validate slave IDs and active status
4. **File Descriptors**: Ensure FDs are valid before operations

## Testing Guide

### 1. **Basic Functionality Test**

```bash
# Build and run automated test
make clean && make debug
./test.sh
```

**Expected behavior**:

- Master starts and creates shared memory
- Slaves register successfully
- Query/response communication works
- Statistics are accurate
- Graceful shutdown on signals

### 2. **Memory Safety Testing**

#### Valgrind Memory Check

```bash
make valgrind-memcheck
```

**Checks for**:

- Memory leaks
- Invalid memory access
- Use of uninitialized memory
- Buffer overflows

#### Expected Clean Output:

```
==PID== HEAP SUMMARY:
==PID==     in use at exit: 0 bytes in 0 blocks
==PID==   total heap usage: N allocs, N frees, X bytes allocated
==PID== 
==PID== All heap blocks were freed -- no leaks are possible
```

### 3. **Thread Safety Testing**

#### Helgrind Thread Analysis

```bash
make valgrind-threads
```

**Detects**:

- Race conditions
- Incorrect mutex usage
- Lock ordering violations
- Data races in shared memory

#### DRD (Data Race Detector)

```bash
make valgrind-drd
```

**More sensitive detection of**:

- Data races
- Lock contention
- Mutex misuse

### 4. **Stress Testing**

#### Multiple Slave Test

```bash
# Start master
./master &
MASTER_PID=$!

# Start many slaves rapidly
for i in {0..9}; do
    ./slave $i &
    sleep 0.1
done

# Trigger stats multiple times
for i in {1..10}; do
    kill -USR1 $MASTER_PID
    sleep 1
done

# Cleanup
killall master slave
```

#### Signal Stress Test

```bash
# Start system
./master &
./slave 0 &
./slave 1 &

# Rapidly send signals
for i in {1..20}; do
    kill -USR1 $(pgrep master)
    sleep 0.1
done

# Test signal handling
kill -TERM $(pgrep slave)
kill -USR1 $(pgrep master)  # Should show updated stats
```

### 5. **Concurrency Testing**

#### Reader/Writer Test

```bash
# Terminal 1: Start system
./master &
./slave 0 &
./slave 1 &

# Terminal 2: Continuous stats reading
while true; do
    ./stats_reader &
    sleep 2
    kill $(pgrep stats_reader)
    sleep 1
done

# Terminal 3: Continuous signal sending
while true; do
    kill -USR1 $(pgrep master)
    sleep 0.5
done
```

### 6. **Error Condition Testing**

#### Missing Master Test

```bash
# Try to start slave without master
./slave 0
# Expected: "open master FIFO: No such file or directory"
```

#### Shared Memory Access Test

```bash
# Try stats_reader without master
./stats_reader
# Expected: "shm_open: No such file or directory"
```

#### Signal Handling Test

```bash
./master &
./slave 0 &

# Test different signals
kill -TERM $(pgrep slave)    # Should unregister gracefully
kill -INT $(pgrep master)    # Should shutdown cleanly
kill -USR1 $(pgrep master)   # Should update stats
```

## Common Pitfalls and Solutions

### 1. **Mutex Initialization Issues**

❌ **Wrong**: Default mutex attributes

```c
pthread_mutex_init(&stats->mutex, NULL);  // Only works within process
```

✅ **Correct**: Process-shared attributes

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_mutex_init(&stats->mutex, &attr);
pthread_mutexattr_destroy(&attr);
```

### 2. **Shared Memory Access Issues**

❌ **Wrong**: Read-only mapping for mutex operations

```c
stats = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);  // Can't lock mutex
```

✅ **Correct**: Read-write mapping

```c
stats = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```

### 3. **Signal Safety Issues**

❌ **Wrong**: Non-atomic signal variables

```c
static int should_exit = 0;  // Not signal-safe
```

✅ **Correct**: Signal-atomic variables

```c
static volatile sig_atomic_t should_exit = 0;
```

### 4. **Resource Cleanup Issues**

❌ **Wrong**: Missing cleanup order

```c
munmap(stats, sizeof(stats_t));
pthread_mutex_destroy(&stats->mutex);  // Accessing freed memory!
```

✅ **Correct**: Proper cleanup order

```c
pthread_mutex_destroy(&stats->mutex);
munmap(stats, sizeof(stats_t));
```

### 5. **FIFO Handling Issues**

❌ **Wrong**: Not handling SIGPIPE

```c
// SIGPIPE can terminate process unexpectedly
```

✅ **Correct**: Ignore SIGPIPE

```c
signal(SIGPIPE, SIG_IGN);
// Handle broken pipes in write() return values
```

## Performance Considerations

### 1. **Mutex Granularity**

- **Current**: One mutex protects entire stats structure
- **Trade-off**: Simple but may cause contention with many slaves
- **Alternative**: Per-slave mutexes (more complex, less contention)

### 2. **Polling vs Blocking I/O**

- **Current**: `poll()` with timeout for responsiveness
- **Benefit**: Allows signal handling and periodic operations
- **Cost**: CPU overhead from periodic wake-ups

### 3. **Memory Access Patterns**

- **Shared memory**: Avoid false sharing between cache lines
- **Stats structure**: Packed for minimal memory footprint
- **Mutex alignment**: First member ensures proper alignment

## Debugging Tips

### 1. **Enable Debug Mode**

```bash
make DEBUG=1  # Enable all assertions
```

### 2. **Check System Resources**

```bash
# View shared memory segments
ls -la /dev/shm/

# View named semaphores
ls -la /dev/shm/sem.*

# View FIFOs
ls -la /tmp/*fifo*
```

### 3. **Process Monitoring**

```bash
# Monitor process relationships
pstree -p $(pgrep master)

# Check file descriptors
lsof -p $(pgrep master)
lsof -p $(pgrep slave)
```

### 4. **Signal Debugging**

```bash
# Send signals manually
kill -USR1 $(pgrep master)  # Trigger stats
kill -TERM $(pgrep slave)   # Graceful slave shutdown
```

## Conclusion

This pthread-based IPC system demonstrates:

- **Process-shared synchronization** using pthread mutexes
- **Signal-safe programming** with atomic variables
- **Resource management** with proper cleanup
- **Memory safety** through careful design
- **Comprehensive testing** covering functionality, memory, and concurrency

The key to success is understanding the subtle differences between thread-based and process-based synchronization, particularly the requirement for `PTHREAD_PROCESS_SHARED` attributes and careful resource cleanup ordering.

## Complete pthread/Semaphore Function Reference

### **Mutex Functions (pthread.h)**

#### `pthread_mutex_init()`

```c
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
```

- **Purpose**: Initialize a mutex with specified attributes
- **Used in**: `master.c:88` - Initialize stats mutex with process-shared attributes
- **Return**: 0 on success, error code on failure
- **Critical**: Must use process-shared attributes for inter-process synchronization

#### `pthread_mutex_lock()`

```c
int pthread_mutex_lock(pthread_mutex_t *mutex);
```

- **Purpose**: Acquire exclusive lock on mutex (blocks if unavailable)
- **Used in**: 
  - `master.c:172, 202, 227, 269, 280` - Protect stats updates
  - `stats_reader.c:106` - Safe reading of shared stats
- **Behavior**: Blocks calling thread until mutex is available
- **Must pair**: Always pair with `pthread_mutex_unlock()`

#### `pthread_mutex_unlock()`

```c
int pthread_mutex_unlock(pthread_mutex_t *mutex);
```

- **Purpose**: Release exclusive lock on mutex
- **Used in**:
  - `master.c:175, 205, 229, 271, 283` - Release after stats updates
  - `stats_reader.c:108` - Release after reading stats
- **Critical**: Must be called by the same thread that acquired the lock
- **Error prone**: Forgetting this causes deadlocks

#### `pthread_mutex_destroy()`

```c
int pthread_mutex_destroy(pthread_mutex_t *mutex);
```

- **Purpose**: Destroy mutex and free associated resources
- **Used in**: `master.c:388` - Cleanup before unmapping shared memory
- **Requirement**: Mutex must be unlocked before destruction
- **Order matters**: Must call before `munmap()` to avoid accessing freed memory

### **Mutex Attribute Functions**

#### `pthread_mutexattr_init()`

```c
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
```

- **Purpose**: Initialize mutex attributes object with default values
- **Used in**: `master.c:84` - Setup attributes for process-shared mutex
- **Must pair**: Always pair with `pthread_mutexattr_destroy()`

#### `pthread_mutexattr_setpshared()`

```c
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
```

- **Purpose**: Set process-shared attribute for mutex
- **Used in**: `master.c:86` - Enable inter-process mutex sharing
- **Values**: 
  - `PTHREAD_PROCESS_PRIVATE` (default) - Same process only
  - `PTHREAD_PROCESS_SHARED` - Across processes
- **Critical**: This is what makes inter-process synchronization possible

#### `pthread_mutexattr_destroy()`

```c
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
```

- **Purpose**: Destroy attributes object and free resources
- **Used in**: `master.c:91` - Cleanup after mutex initialization
- **When**: Call after `pthread_mutex_init()` completes

### **Semaphore Functions (semaphore.h)**

#### `sem_open()` - Create/Open Named Semaphore

```c
sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned value);
sem_t *sem_open(const char *name, int oflag);
```

- **Purpose**: Create or open a named semaphore
- **Used in**:
  - `master.c:120` - Create semaphore: `sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0)`
  - `master.c:112` - Test existing: `sem_open(SEM_NAME, 0)`
  - `stats_reader.c:84` - Open existing: `sem_open(SEM_NAME, 0)`
- **Flags**:
  - `O_CREAT | O_EXCL` - Create new (fail if exists)
  - `0` - Open existing only
- **Initial value**: 0 (readers block until master posts)

#### `sem_post()`

```c
int sem_post(sem_t *sem);
```

- **Purpose**: Increment semaphore value (signal/wake up waiters)
- **Used in**: `master.c:299` - Signal that stats are ready
- **Effect**: Wakes up one waiting thread/process
- **Never blocks**: Always returns immediately

#### `sem_timedwait()`

```c
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
```

- **Purpose**: Wait for semaphore with timeout
- **Used in**: `stats_reader.c:102` - Wait for stats update with 2-second timeout
- **Behavior**: 
  - Decrements semaphore if > 0
  - Blocks if semaphore is 0, until timeout or signal
- **Returns**: 0 on success, -1 on timeout (errno = ETIMEDOUT)

#### `sem_close()`

```c
int sem_close(sem_t *sem);
```

- **Purpose**: Close semaphore handle (per-process cleanup)
- **Used in**:
  - `master.c:114` - Close test handle
  - `master.c:396` - Close main handle during cleanup
  - `stats_reader.c:117` - Close reader handle
- **Note**: Doesn't remove semaphore from system (use `sem_unlink` for that)

#### `sem_unlink()`

```c
int sem_unlink(const char *name);
```

- **Purpose**: Remove named semaphore from system
- **Used in**:
  - `master.c:109` - Remove old semaphore before creating new
  - `master.c:399` - Final cleanup during shutdown
- **Effect**: Semaphore is destroyed when all processes close it

### **Signal-Safe Types (signal.h)**

#### `sig_atomic_t`

```c
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t notify_master = 0;
static volatile sig_atomic_t dump_stats = 0;
```

- **Purpose**: Type guaranteed to be atomic for signal handler communication
- **Used in**: All source files for signal-safe flags
- **Why needed**: Signal handlers can't safely use mutexes or complex operations
- **Always with volatile**: Prevents compiler optimization issues

### **Memory Constants and Macros**

#### `PTHREAD_PROCESS_SHARED`

- **Purpose**: Constant for `pthread_mutexattr_setpshared()`
- **Value**: Implementation-defined constant
- **Effect**: Makes mutex work across process boundaries
- **Alternative**: `PTHREAD_PROCESS_PRIVATE` (default, same process only)

#### `SEM_FAILED`

- **Purpose**: Return value indicating semaphore operation failure
- **Value**: `(sem_t *)(-1)` typically
- **Usage**: Compare return values from `sem_open()`
- **Example**: `if (stats_ready_sem == SEM_FAILED) { /* handle error */ }`

## Function Usage Patterns in Project

### **Mutex Initialization Pattern**

```c
// 1. Create attributes
pthread_mutexattr_t mutex_attr;
pthread_mutexattr_init(&mutex_attr);

// 2. Set process-shared
pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

// 3. Initialize mutex
pthread_mutex_init(&stats->mutex, &mutex_attr);

// 4. Cleanup attributes
pthread_mutexattr_destroy(&mutex_attr);
```

### **Critical Section Pattern**

```c
// Always follow this pattern - no exceptions
pthread_mutex_lock(&stats->mutex);
// ... modify shared data ...
stats->messages_sent[id]++;
stats->active_slaves[id] = 1;
// ... end modifications ...
pthread_mutex_unlock(&stats->mutex);
```

### **Semaphore Signaling Pattern**

```c
// Master: Signal stats ready
pthread_mutex_lock(&stats->mutex);
// ... update stats ...
pthread_mutex_unlock(&stats->mutex);
sem_post(stats_ready_sem);  // Signal after releasing mutex

// Reader: Wait for signal
sem_timedwait(stats_ready_sem, &timeout);
pthread_mutex_lock((pthread_mutex_t *)&stats->mutex);
// ... read stats safely ...
pthread_mutex_unlock((pthread_mutex_t *)&stats->mutex);
```

### **Cleanup Order Pattern**

```c
// CORRECT order: mutex first, then memory
pthread_mutex_destroy(&stats->mutex);
munmap(stats, sizeof(stats_t));

// WRONG order: accessing freed memory!
// munmap(stats, sizeof(stats_t));
// pthread_mutex_destroy(&stats->mutex);  // ERROR!
```
