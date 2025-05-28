# Master-Slave IPC System

This project implements a master-slave inter-process communication system using FIFOs, shared memory, and semaphores in C.

## Architecture

- **Master Process**: Manages slave registration, sends periodic queries, and maintains statistics in shared memory
- **Slave Processes**: Register with master, process queries, and send responses
- **Stats Reader**: External process that reads statistics from shared memory

## Features

- FIFO-based communication between master and slaves
- Shared memory for statistics storage with mutex synchronization
- Semaphore-based signaling for statistics updates
- Signal handling for graceful shutdown
- Debug assertions for development/testing
- Comprehensive error handling

## Building

```bash
make            # Build debug version (default)
make release    # Build optimized release version
make clean      # Clean build artifacts
```

## Usage

### Start the master process:
```bash
./master
```

### Start slave processes:
```bash
./slave 0   # Start slave with ID 0
./slave 1   # Start slave with ID 1
./slave 2   # Start slave with ID 2
```

### View statistics:
```bash
./stats_reader  # Will wait for statistics updates
```

### Trigger statistics update:
```bash
kill -USR1 <master_pid>  # Send SIGUSR1 to master
```

## Testing

```bash
make test       # Run automated test
./test.sh       # Run test script directly
```

## Advanced Testing

```bash
make valgrind-memcheck   # Memory leak detection
make valgrind-threads    # Thread safety analysis
make valgrind-drd        # Data race detection
```

## Implementation Details

### Communication Flow
1. Slaves create their own FIFOs and register with master via master FIFO
2. Master opens each slave's FIFO for sending queries
3. Master sends periodic queries with incrementing values
4. Slaves process queries (double the value) and send responses
5. Statistics are maintained in shared memory with mutex protection

### Signal Handling
- `SIGINT`/`SIGTERM`: Graceful shutdown
- `SIGUSR1`: Trigger statistics dump (master only)
- `SIGPIPE`: Ignored (broken pipe handling)

### IPC Mechanisms
- **FIFOs**: Message passing between master and slaves
- **Shared Memory**: Statistics storage accessible by external readers
- **Semaphores**: Notification mechanism for statistics updates
- **Mutexes**: Thread-safe access to shared statistics

### Debug Mode
The system includes extensive debug assertions that can be disabled by setting `DEBUG=0` during compilation.

## Files

- `include/common.h`: Shared definitions and structures
- `src/master.c`: Master process implementation
- `src/slave.c`: Slave process implementation  
- `src/stats_reader.c`: Statistics reader implementation
- `Makefile`: Build configuration
- `test.sh`: Automated test script
