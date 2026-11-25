# POSIX Process Watchdog

System Programming Course Project

## 1. Project Overview

The POSIX Process Watchdog is a process supervision system developed in C for Linux-based operating systems.

The main purpose of this project is to monitor worker processes, detect unexpected terminations, automatically restart failed workers, and provide a safe shutdown mechanism.

The project demonstrates fundamental system programming concepts, including process management, POSIX signals, inter-process communication, logging, and automated system testing.

## 2. Features

- Creation and management of multiple worker processes
- Automatic detection of terminated processes
- Automatic restart of unexpectedly terminated workers
- Configurable restart limit through header constants
- Periodic worker monitoring
- On-demand worker status reporting
- Graceful shutdown mechanism
- Zombie process prevention through process reaping
- Signal mask management for restarted workers
- Timestamped logging with INFO, WARN, and ERROR levels
- Automated system and regression testing
- Makefile-based build system

## 3. System Architecture

The system consists of four main modules:

| Module | Responsibility |
|--------|----------------|
| Main | Coordinates the system and main control loop |
| Signal Manager | Handles POSIX signals and periodic monitoring |
| Process Manager | Creates, monitors, restarts, and terminates workers |
| Logger | Records system events and worker status information |

The Watchdog operates as the parent process and supervises two child worker processes.

```text
                  +-------------------+
                  |     Watchdog      |
                  |  Parent Process   |
                  +-------------------+
                           |
          +----------------+----------------+
          |                |                |
   Signal Manager   Process Manager       Logger
                           |                |
                    +------+------+    watchdog.log
                    |             |
                 Worker 1      Worker 2
```

Worker processes are created using `fork()` and `exec()`.

The Watchdog monitors their execution using POSIX signals and `waitpid()`.

A pipe is used to communicate execution failures from the child process to the parent process.

## 4. Project Structure

```text
watchdog-project/
|
|-- include/
|   |-- logger.h
|   |-- process_manager.h
|   |-- signal_manager.h
|
|-- src/
|   |-- main.c
|   |-- logger.c
|   |-- process_manager.c
|   |-- signal_manager.c
|
|-- workers/
|   |-- worker.c
|
|-- tests/
|   |-- test_scenarios.sh
|   |-- test_termination.c
|   |-- test_periodic_restart.c
|   |-- test_exec_failure.c
|   |-- test_shutdown.c
|   |-- test_echild.c
|   |-- test_shutdown_restart.c
|   |-- test_restart.c
|   |-- test_periodic.c
|
|-- test_process.c
|-- Makefile
|-- README.md
|-- .gitignore
```

## 5. Requirements

- Linux operating system or Windows Subsystem for Linux (WSL)
- GCC compiler with C11 support
- GNU Make
- Bash
- Git
- Standard Linux utilities such as grep, awk, ps, and mktemp

For Ubuntu-based systems, the required development tools can be installed using:

```bash
sudo apt update
sudo apt install build-essential git
```

## 6. Installation and Build

Clone the repository:

```bash
git clone https://github.com/EzgiYz/watchdog-project.git
cd watchdog-project
```

Compile the project:

```bash
make
```

The build process generates two executables:

- `watchdog`
- `worker`

The Makefile uses the following compiler flags:

```text
-std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
```

Header dependencies are automatically generated using the GCC dependency generation options.

### Makefile Targets

| Target | Description |
|--------|-------------|
| make | Builds the Watchdog and worker executables |
| make all | Builds all required executables |
| make clean | Removes generated build files |
| make test | Builds the project and runs automated system tests |

To rebuild the entire project:

```bash
make clean
make
```

## 7. Running the Application

Start the Watchdog from the project root directory:

```bash
./watchdog
```

Example output:

```text
Watchdog starting...
Worker 1 started with PID 1250
Worker 2 started with PID 1251
Watchdog running. PID: 1249
Press Ctrl+C to stop.
```

The actual process IDs will be different on each run.

The Watchdog continues monitoring its worker processes until a shutdown signal is received.

To stop the application, press `Ctrl+C` or send SIGTERM to the Watchdog process.

```bash
kill -SIGTERM <watchdog_pid>
```

Replace `<watchdog_pid>` with the actual Watchdog PID.

## 8. Signal Handling

The project uses the following POSIX signals:

| Signal | Purpose |
|--------|---------|
| SIGCHLD | Notifies the Watchdog of child process termination |
| SIGALRM | Triggers periodic worker monitoring |
| SIGUSR1 | Requests a worker status report |
| SIGINT | Requests graceful Watchdog shutdown |
| SIGTERM | Requests controlled termination |
| SIGKILL | Forces process termination when necessary |

Signal handlers only update `volatile sig_atomic_t` flags.

Complex operations such as logging, process management, and status reporting are performed outside signal handlers in the main control flow.

The Watchdog uses signal masks and `sigsuspend()` to coordinate signal processing.

### Requesting a Status Report

Open a second terminal and execute:

```bash
kill -SIGUSR1 <watchdog_pid>
```

Replace `<watchdog_pid>` with the actual Watchdog PID.

Example status report:

```text
========== WATCHDOG STATUS ==========
Worker 1 | PID: 1250 | Running: YES | Restarts: 0
Worker 2 | PID: 1251 | Running: YES | Restarts: 0
=====================================
```

The status report is displayed in the terminal and recorded in the log file.

## 9. Worker Restart Policy

The current configuration supports two worker processes and a maximum of three restart attempts per worker.

These values are defined in `include/process_manager.h`:

```c
#define MAX_WORKERS 2
#define MAX_RESTARTS 3
```

The restart policy is as follows:

- A worker exiting with code 0 is considered normally terminated and is not restarted.
- A worker exiting with a non-zero exit code is considered unexpectedly terminated.
- A worker terminated by a signal is considered unexpectedly terminated.
- Unexpectedly terminated workers are restarted if their restart limit has not been reached.
- Each worker has an independent restart counter.
- After three restart attempts, no additional restart is performed.
- Worker restarts are disabled during system shutdown.

### Testing a Normal Termination

Send SIGTERM to an active worker:

```bash
kill -SIGTERM <worker_pid>
```

The worker handles SIGTERM and exits with code 0.

The Watchdog detects the normal termination and does not restart the worker.

### Testing an Unexpected Termination

Send SIGKILL to an active worker:

```bash
kill -SIGKILL <worker_pid>
```

The Watchdog detects the unexpected termination and attempts to create a replacement worker.

The replacement worker receives a new process ID, and the restart counter is incremented.

### Restart Limit

When the maximum restart limit is reached, the worker is no longer restarted.

The following error message is recorded in the log file:

```text
[ERROR] Worker 1 restart limit reached!
```

The Watchdog continues monitoring the remaining active workers.

## 10. Graceful Shutdown

When the Watchdog receives SIGINT or SIGTERM:

1. Worker restarts are disabled.
2. The periodic monitoring timer is stopped.
3. SIGTERM is sent to active worker processes.
4. The Watchdog waits approximately three seconds for workers to terminate.
5. Remaining workers are forcefully terminated using SIGKILL when necessary.
6. Child processes are reaped to prevent zombies.
7. Shutdown events are recorded in the log file.
8. The logging system is closed.

Worker processes handle SIGTERM by updating a termination flag and exiting normally with code 0.

### Signal Mask Management

Child processes inherit the signal mask of their parent process during `fork()`.

The signal mask is also preserved across `exec()`.

During development, a signal mask inheritance issue was identified in restarted workers. Since the Watchdog blocks SIGTERM during its main control loop, restarted workers could inherit this blocked signal.

As a result, restarted workers could not terminate normally when receiving SIGTERM.

To resolve this issue, each worker explicitly unblocks SIGTERM after registering its signal handler.

```c
sigset_t signals;

sigemptyset(&signals);
sigaddset(&signals, SIGTERM);

if (sigprocmask(SIG_UNBLOCK, &signals, NULL) == -1) {
    perror("sigprocmask failed");
    return 1;
}
```

This ensures that both initially created workers and restarted workers can respond to SIGTERM.

A regression test verifies that restarted workers can terminate normally without triggering another restart.

## 11. Logging System

The Logger module records system events in:

```text
watchdog.log
```

Each log entry contains a timestamp, a severity level, and an event description.

### Log Format

```text
[YYYY-MM-DD HH:MM:SS] [LEVEL] Message
```

### Log Levels

| Level | Description |
|-------|-------------|
| INFO | Normal system operations |
| WARN | Unexpected termination and warning events |
| ERROR | System errors and restart limit violations |

### Logger Interface

The Logger module provides the following functions:

| Function | Description |
|----------|-------------|
| logger_init() | Initializes the logging system and opens the log file |
| logger_log() | Writes a timestamped log message |
| logger_status() | Records the current status of all workers |
| logger_close() | Closes the log file and releases resources |

### Logged Events

The Logger records:

- Watchdog startup and shutdown
- Worker process creation
- Normal worker termination
- Unexpected worker termination
- Worker exit codes
- Worker termination signals
- Worker restart attempts
- Restart limit violations
- SIGUSR1 status requests
- Worker status reports
- Process management errors
- SIGTERM delivery during shutdown
- Forced termination attempts
- Process reaping events

### Example Log Output

```text
[2026-11-25 18:43:01] [WARN] Worker 1 (PID 13822) terminated by signal 9
[2026-11-25 18:43:01] [INFO] Restarting Worker 1 (1/3)
[2026-11-25 18:43:01] [INFO] Worker 1 started with PID 13986
[2026-11-25 18:43:49] [INFO] Worker 1 (PID 13986) exited with code 0
```

### Example Shutdown Log

```text
[2026-11-25 18:33:13] [INFO] Shutdown requested.
[2026-11-25 18:33:13] [INFO] SIGTERM sent to Worker 1 (PID 11041)
[2026-11-25 18:33:13] [INFO] SIGTERM sent to Worker 2 (PID 11042)
[2026-11-25 18:33:14] [INFO] Worker 1 (PID 11041) exited with code 0
[2026-11-25 18:33:14] [INFO] Worker 2 (PID 11042) exited with code 0
[2026-11-25 18:33:14] [INFO] Watchdog stopped.
```

### File Management and Error Handling

The log file is opened in append mode, allowing records from previous executions to be preserved.

Log messages are flushed after each entry so that events can be monitored in real time.

The logging module checks the following operations for errors:

- File opening
- File descriptor configuration
- Timestamp generation
- Log message writing
- File flushing
- File closing

The log file descriptor is configured with `FD_CLOEXEC` to prevent worker executables from retaining the Watchdog's log file descriptor after `exec()`.

Logging operations are performed outside signal handlers.

### Viewing Log Records

To view the entire log file:

```bash
cat watchdog.log
```

To view the last ten log entries:

```bash
tail -n 10 watchdog.log
```

To monitor log entries in real time:

```bash
tail -f watchdog.log
```

## 12. Automated Testing

The automated test script is located at:

```text
tests/test_scenarios.sh
```

Run the automated system scenarios using:

```bash
make test
```

The script creates an isolated temporary directory using `mktemp`.

The Watchdog and worker executables are copied into this directory to prevent test executions from modifying the main application log file.

The script automatically:

- Starts the Watchdog
- Extracts worker process IDs from log records
- Sends POSIX signals
- Waits for expected log messages
- Verifies process states
- Checks restart counters
- Validates log formatting
- Reports test results
- Attempts to clean up remaining test processes
- Removes temporary test files

### Test Scenarios

| Test | Description | Result |
|------|-------------|--------|
| 1 | Normal startup | PASS |
| 2 | SIGUSR1 status report | PASS |
| 3 | Normal worker termination without restart | PASS |
| 4 | Unexpected termination and automatic restart | PASS |
| 5 | Restart limit enforcement | PASS |
| 6 | Graceful shutdown and process cleanup | PASS |
| 7 | Log format and log levels | PASS |
| 8 | Normal termination after automatic restart | PASS |

### Test Output

```text
========================================
       WATCHDOG AUTOMATED TESTS
========================================
[PASS] Normal startup
[PASS] SIGUSR1 status report
[PASS] Normal worker termination without restart
[PASS] Unexpected termination and automatic restart
[PASS] Restart limit enforcement
[PASS] Graceful shutdown and process cleanup
[PASS] Log format and log levels
[PASS] Normal termination after automatic restart

========================================
All tests passed: 8/8
========================================
```

All eight automated system scenarios were successfully executed in the Linux environment provided by WSL.

### Regression Testing

During development, restarted workers were found to inherit a blocked SIGTERM signal from the Watchdog process.

After resolving this issue, an additional regression test was implemented.

This test verifies that:

1. A worker is unexpectedly terminated.
2. The Watchdog successfully restarts the worker.
3. The restarted worker receives SIGTERM.
4. The worker exits normally with code 0.
5. No additional restart is performed.
6. The other worker continues running.
7. The Watchdog shuts down successfully.

This regression test helps prevent the signal mask inheritance issue from reappearing in future versions.

## 13. Team Responsibilities

| Team Member | Responsibility |
|-------------|----------------|
| Serra | Signal Management and Main Control Flow |
| Ezgi | Process Management |
| Ayberk | Logging and Test Infrastructure |

### Signal Management and Main Control Flow

- POSIX signal handlers
- Signal flags and signal masks
- Periodic monitoring timer
- Main control loop
- Graceful shutdown coordination

### Process Management

- Worker process creation
- Process monitoring
- Unexpected termination detection
- Automatic restart mechanism
- Restart limit enforcement
- Child process cleanup

### Logging and Test Infrastructure

- Logger module implementation
- Timestamp and severity level formatting
- Log file management and error handling
- Logging integration with system modules
- Worker status logging
- Makefile implementation
- Automated system test scenarios
- Regression testing
- Signal mask issue investigation and correction
- Project documentation
- Git ignore configuration

## 14. Known Limitations

- The project targets Linux and POSIX-compatible environments.
- The number of workers and restart limit are defined at compile time.
- The system monitors process termination but does not detect unresponsive worker processes.
- Log rotation is not implemented.
- The automated system tests currently assume two workers and a maximum of three restart attempts.
- The automated system tests do not cover every possible operating system failure.
- Logger file access and write failure scenarios require additional dedicated testing.
- Forced shutdown has not been fully covered by automated tests.
- The additional C test files are not executed by the current `make test` target.
- Worker initialization and signal delivery may be subject to timing-related race conditions.

## 15. Conclusion

The POSIX Process Watchdog demonstrates how process supervision can be implemented using fundamental Linux system programming mechanisms.

The project combines process management, POSIX signal handling, inter-process communication, logging, automatic recovery, graceful shutdown, and automated testing into a modular application.

The successful execution of all eight automated system test scenarios demonstrates that the main system functionalities operate as expected under the tested conditions.