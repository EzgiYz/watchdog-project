#!/usr/bin/env bash

set -euo pipefail

# Project directory
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Isolated test environment
TEST_DIR="$(mktemp -d)"
LOG_FILE="$TEST_DIR/watchdog.log"

WATCHDOG_PID=""
WORKER_PIDS=()
PASSED=0

# Terminal colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# Clean up processes and temporary files
cleanup() {
    if [[ -n "$WATCHDOG_PID" ]]; then
        kill -TERM "$WATCHDOG_PID" 2>/dev/null || true

        for ((i=0; i<50; i++)); do
            if ! kill -0 "$WATCHDOG_PID" 2>/dev/null; then
                break
            fi

            sleep 0.1
        done

        if kill -0 "$WATCHDOG_PID" 2>/dev/null; then
            # Stop remaining child processes
            while read -r pid; do
                [[ -n "$pid" ]] || continue
                kill -KILL "$pid" 2>/dev/null || true
            done < <(ps -o pid= --ppid "$WATCHDOG_PID")

            kill -KILL "$WATCHDOG_PID" 2>/dev/null || true
        fi

        wait "$WATCHDOG_PID" 2>/dev/null || true
    fi

    rm -rf "$TEST_DIR"
}

# Print successful test result
pass() {
    PASSED=$((PASSED + 1))
    printf "${GREEN}[PASS]${NC} %s\n" "$1"
}

# Print failed test result
fail() {
    printf "${RED}[FAIL]${NC} %s\n" "$1"

    echo
    echo "Watchdog log:"

    if [[ -f "$LOG_FILE" ]]; then
        cat "$LOG_FILE"
    fi

    exit 1
}

# Wait for a specific log message
wait_for_log() {
    local pattern="$1"

    for ((i=0; i<100; i++)); do
        if [[ -f "$LOG_FILE" ]] &&
           grep -Eq "$pattern" "$LOG_FILE"; then
            return 0
        fi

        sleep 0.1
    done

    fail "Expected log not found: $pattern"
}

# Check whether a process exists
process_exists() {
    kill -0 "$1" 2>/dev/null
}

# Extract the latest PID of a worker
get_worker_pid() {
    local worker_id="$1"

    awk -v id="$worker_id" \
        '$4 == "Worker" && $5 == id &&
         $6 == "started" {pid=$NF}
         END {print pid}' "$LOG_FILE"
}

# Check required executables
[[ -x "$ROOT_DIR/watchdog" ]] ||
    fail "Watchdog executable not found."

[[ -x "$ROOT_DIR/worker" ]] ||
    fail "Worker executable not found."

cp "$ROOT_DIR/watchdog" "$TEST_DIR/"
cp "$ROOT_DIR/worker" "$TEST_DIR/"

cd "$TEST_DIR"

echo "========================================"
echo "       WATCHDOG AUTOMATED TESTS"
echo "========================================"

# TEST 1: Normal startup
./watchdog > watchdog_output.txt 2>&1 &
WATCHDOG_PID=$!

wait_for_log "Watchdog running"
wait_for_log "Worker 1 started with PID"
wait_for_log "Worker 2 started with PID"

WORKER1_PID="$(get_worker_pid 1)"
WORKER2_PID="$(get_worker_pid 2)"

WORKER_PIDS+=("$WORKER1_PID" "$WORKER2_PID")

process_exists "$WATCHDOG_PID" ||
    fail "Watchdog is not running."

process_exists "$WORKER1_PID" ||
    fail "Worker 1 is not running."

process_exists "$WORKER2_PID" ||
    fail "Worker 2 is not running."

pass "Normal startup"

# TEST 2: SIGUSR1 status report
kill -USR1 "$WATCHDOG_PID"

wait_for_log "SIGUSR1 status request received"
wait_for_log "Worker status report"
wait_for_log "Worker 1 .*Running: YES.*Restarts: 0"
wait_for_log "Worker 2 .*Running: YES.*Restarts: 0"

pass "SIGUSR1 status report"

# TEST 3: Normal worker termination
kill -TERM "$WORKER2_PID"

wait_for_log \
    "Worker 2 \(PID $WORKER2_PID\) exited with code 0"

kill -USR1 "$WATCHDOG_PID"

wait_for_log \
    "Worker 2 .*PID: 0.*Running: NO.*Restarts: 0"

pass "Normal worker termination without restart"

# TEST 4: Unexpected termination and restart
OLD_PID="$WORKER1_PID"

kill -KILL "$OLD_PID"

wait_for_log "Restarting Worker 1 \(1/3\)"

for ((i=0; i<100; i++)); do
    WORKER1_PID="$(get_worker_pid 1)"

    if [[ "$WORKER1_PID" != "$OLD_PID" ]]; then
        break
    fi

    sleep 0.1
done

[[ "$WORKER1_PID" != "$OLD_PID" ]] ||
    fail "Worker PID did not change."

WORKER_PIDS+=("$WORKER1_PID")

process_exists "$WORKER1_PID" ||
    fail "Restarted worker is not running."

pass "Unexpected termination and automatic restart"

# TEST 5: Restart limit
for restart in 2 3; do
    OLD_PID="$WORKER1_PID"

    kill -KILL "$OLD_PID"

    wait_for_log "Restarting Worker 1 \($restart/3\)"

    for ((i=0; i<100; i++)); do
        WORKER1_PID="$(get_worker_pid 1)"

        if [[ "$WORKER1_PID" != "$OLD_PID" ]]; then
            break
        fi

        sleep 0.1
    done

    [[ "$WORKER1_PID" != "$OLD_PID" ]] ||
        fail "Worker restart failed."

    WORKER_PIDS+=("$WORKER1_PID")

    process_exists "$WORKER1_PID" ||
        fail "Restarted worker is not running."
done

kill -KILL "$WORKER1_PID"

wait_for_log "Worker 1 restart limit reached"

kill -USR1 "$WATCHDOG_PID"

wait_for_log \
    "Worker 1 .*PID: 0.*Running: NO.*Restarts: 3"

pass "Restart limit enforcement"

# Stop the first watchdog instance
kill -TERM "$WATCHDOG_PID"
wait_for_log "Watchdog stopped"
wait "$WATCHDOG_PID"

WATCHDOG_PID=""

# TEST 6: Graceful shutdown with active workers
cp "$LOG_FILE" "$TEST_DIR/previous.log"
: > "$LOG_FILE"

./watchdog >> watchdog_output.txt 2>&1 &
WATCHDOG_PID=$!

wait_for_log "Watchdog running"

WORKER1_PID="$(get_worker_pid 1)"
WORKER2_PID="$(get_worker_pid 2)"

WORKER_PIDS+=("$WORKER1_PID" "$WORKER2_PID")

# Allow workers to initialize their signal handlers
sleep 1

kill -TERM "$WATCHDOG_PID"

wait_for_log "Shutdown requested"
wait_for_log "Watchdog stopped"

wait "$WATCHDOG_PID"

process_exists "$WATCHDOG_PID" &&
    fail "Watchdog is still running."

process_exists "$WORKER1_PID" &&
    fail "Worker 1 is still running."

process_exists "$WORKER2_PID" &&
    fail "Worker 2 is still running."

wait_for_log \
    "Worker 1 \(PID $WORKER1_PID\) exited with code 0"

wait_for_log \
    "Worker 2 \(PID $WORKER2_PID\) exited with code 0"

WATCHDOG_PID=""

pass "Graceful shutdown and process cleanup"

# TEST 7: Log format and log levels
cat "$TEST_DIR/previous.log" "$LOG_FILE" \
    > "$TEST_DIR/combined.log"

for level in INFO WARN ERROR; do
    grep -q "\[$level\]" "$TEST_DIR/combined.log" ||
        fail "Missing log level: $level"
done

LOG_PATTERN='^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] \[(INFO|WARN|ERROR)\] .+$'

if grep -Evq "$LOG_PATTERN" "$TEST_DIR/combined.log"; then
    fail "Invalid log format."
fi

pass "Log format and log levels"
# TEST 8: Normal termination after automatic restart
: > "$LOG_FILE"

./watchdog >> watchdog_output.txt 2>&1 &
WATCHDOG_PID=$!

wait_for_log "Watchdog running"

WORKER1_PID="$(get_worker_pid 1)"
WORKER2_PID="$(get_worker_pid 2)"

WORKER_PIDS+=("$WORKER1_PID" "$WORKER2_PID")

OLD_PID="$WORKER1_PID"

kill -KILL "$OLD_PID"

wait_for_log "Restarting Worker 1 \(1/3\)"

for ((i=0; i<100; i++)); do
    WORKER1_PID="$(get_worker_pid 1)"

    if [[ -n "$WORKER1_PID" &&
          "$WORKER1_PID" != "$OLD_PID" ]]; then
        break
    fi

    sleep 0.1
done

[[ -n "$WORKER1_PID" &&
   "$WORKER1_PID" != "$OLD_PID" ]] ||
    fail "Worker restart failed."

WORKER_PIDS+=("$WORKER1_PID")

# Allow the restarted worker to initialize
sleep 1

kill -TERM "$WORKER1_PID"

wait_for_log \
    "Worker 1 \(PID $WORKER1_PID\) exited with code 0"

kill -USR1 "$WATCHDOG_PID"

wait_for_log \
    "Worker 1 .*PID: 0.*Running: NO.*Restarts: 1"

if process_exists "$WORKER1_PID"; then
    fail "Restarted worker is still running."
fi

if grep -q "Restarting Worker 1 (2/3)" "$LOG_FILE"; then
    fail "Normally terminated worker was restarted."
fi

process_exists "$WORKER2_PID" ||
    fail "Worker 2 unexpectedly stopped."

kill -TERM "$WATCHDOG_PID"

wait_for_log "Watchdog stopped"
wait "$WATCHDOG_PID"

if process_exists "$WORKER2_PID"; then
    fail "Worker 2 is still running after shutdown."
fi

WATCHDOG_PID=""

pass "Normal termination after automatic restart"

echo
echo "========================================"
printf "${GREEN}All tests passed: %d/8${NC}\n" "$PASSED"
echo "========================================"

exit 0