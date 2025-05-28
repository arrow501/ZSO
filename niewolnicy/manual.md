# pthread Developer Manual: Master-Slave IPC System

## Table of Contents
1. [Overview](#overview)
2. [pthread Concepts Used](#pthread-concepts-used)
3. [Memory and Thread Safety Design](#memory-and-thread-safety-design)
4. [Debug Assertions](#debug-assertions)
5. [Testing Guide](#testing-guide)
6. [Common Pitfalls and Solutions](#common-pitfalls-and-solutions)

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
└─────────┘                      │         │     Shared Memory
                                 │ Master  │ ←→ ┌──────────────┐
┌─────────┐     Named Pipe      │         │    │   Statistics │
│ Slave 1 │ ←─────────────────→ │         │    │   + Mutex    │
└─────────┘                      └─────────┘    └──────────────┘
                                      ↑                ↑
                                      │                │
                                   Semaphore    ┌──────────────┐
                                                │ Stats Reader │
                                                └──────────────┘
```

## pthread Concepts Used

### 1. **pthread_mutex_t** - Mutual Exclusion
```c
pthread_mutex_t mutex;  // Protects shared memory stats
```
- **Purpose**: Prevents race conditions when multiple processes access shared data
- **Key functions**:
  - `pthread_mutex_init()` - Initialize mutex (with PTHREAD_PROCESS_SHARED)
  - `pthread_mutex_lock()` - Acquire exclusive access
  - `pthread_mutex_unlock()` - Release exclusive access
  - `pthread_mutex_destroy()` - Clean up mutex

### 2. **pthread_mutexattr_t** - Mutex Attributes
```c
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
```
- **Purpose**: Configure mutex to work across process boundaries
- **Critical**: Without PTHREAD_PROCESS_SHARED, mutex only works within a single process

### 3. **Shared Memory** - mmap with MAP_SHARED
```c
stats = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```
- **Purpose**: Memory region accessible by multiple processes
- **Key**: Must be paired with process-shared mutex for thread safety

### 4. **POSIX Semaphores** - sem_t
```c
sem_t *stats_ready_sem;  // Named semaphore for signaling
```
- **Purpose**: Synchronization primitive for signaling between processes
- **Functions**:
  - `sem_open()` - Create/open named semaphore
  - `sem_post()` - Signal (increment)
  - `sem_wait()/sem_timedwait()` - Wait for signal
  - `sem_close()` - Close semaphore
  - `sem_unlink()` - Remove named semaphore

## Memory and Thread Safety Design

### Safety Principles

1. **Lock Ordering# pthread Developer Manual: Master-Slave IPC System
