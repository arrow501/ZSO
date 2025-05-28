# Master-Slave IPC System

A Linux inter-process communication (IPC) system demonstrating FIFOs, shared memory, semaphores, and signal handling.

## Overview

This project implements a master-slave architecture where:
- **Master process**: Manages slave connections, sends queries, and maintains statistics
- **Slave processes**: Register with master, process queries, and respond  
- **Stats reader**: External process that reads statistics from shared memory

## Features

- **Named FIFOs**: Communication channels between processes
- **Shared Memory**: Statistics storage accessible by multiple processes
- **POSIX Semaphores**: Synchronization for stats updates
- **Signal Handling**: Graceful shutdown and stats dumping
- **Process-shared Mutexes**: Thread-safe access to shared data
- **Comprehensive Debug Assertions**: Extensive validation in debug builds

## Architecture

### Communication Flow
1. Master creates a named FIFO and waits for connections
2. Slaves create their own FIFOs and register with master
3. Master sends periodic queries to all registered slaves
4. Slaves process queries and send responses back
5. Statistics are maintained in shared memory
6. External stats reader can monitor the system

### IPC Mechanisms Used
- **Named FIFOs**: `/tmp/master_fifo`, `/tmp/slave_fifo_N`
- **Shared Memory**: `/master_stats` 
- **Named Semaphore**: `/stats_ready`
- **Signals**: SIGINT/SIGTERM (shutdown), SIGUSR1 (stats dump)

## Building

```bash
# Debug build (default - includes assertions)
make

# Release build (optimized, no assertions)
make release

# Clean build artifacts
make clean
```

## Usage

### Basic Operation

1. **Start the master process:**
   ```bash
   ./master
   ```

2. **Start slave processes (in separate terminals):**
   ```bash
   ./slave 0
   ./slave 1  
   ./slave 2
   ```

3. **Start stats reader (optional):**
   ```bash
   ./stats_reader
   ```

4. **Trigger stats display:**
   ```bash
   kill -USR1 <master_pid>
   ```

### Automated Testing

```bash
# Run the test script
make test
```

## Files

### Core Implementation
- `include/common.h` - Shared definitions and structures
- `src/master.c` - Master process implementation  
- `src/slave.c` - Slave process implementation
- `src/stats_reader.c` - External statistics reader

### Build System
- `Makefile` - Build configuration with debug/release modes
- `test.sh` - Automated test script

## Configuration

Key constants in `common.h`:
- `MAX_SLAVES` - Maximum number of slave processes (10)
- `POLL_TIMEOUT_MS` - Polling timeout (100ms)
- `QUERY_INTERVAL_MS` - Query sending interval (1000ms)

## Debug Features

When built in debug mode (`DEBUG=1`):
- Extensive assertions validate system state
- Detailed error messages with file/line information
- Debug output for key operations

## Memory and Thread Safety

- Process-shared mutexes protect shared memory access
- Proper cleanup on exit using `atexit()` handlers
- Signal-safe operations in signal handlers
- Comprehensive resource management

## Testing

The project includes several testing modes:

```bash
# Basic functionality test
make test

# Memory leak detection  
make valgrind-memcheck

# Thread synchronization check
make valgrind-threads

# Data race detection
make valgrind-drd
```

## Signal Handling

- **SIGINT/SIGTERM**: Graceful shutdown of all processes
- **SIGUSR1**: Trigger statistics update (master only)
- **SIGPIPE**: Ignored to handle broken pipe conditions

## Error Handling

The system includes robust error handling:
- File descriptor validation
- Shared memory initialization checks
- Message validation with magic numbers
- Graceful degradation on slave disconnection

## Compatibility

- Requires Linux with POSIX IPC support
- Uses GNU C extensions (`-D_GNU_SOURCE`)
- Tested with GCC and standard Linux distributions
