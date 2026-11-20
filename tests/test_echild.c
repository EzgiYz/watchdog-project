#include "process_manager.h"

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

int main(void)
{
    printf("TEST: ECHILD error handling\n");

    if (start_workers() != 0)
    {
        printf("FAILED: Could not start workers!\n");
        return 1;
    }

    const Worker *workers = get_workers();
    pid_t original_pid = workers[0].pid;

    printf("Terminating Worker 1 (PID %ld)...\n",
           (long)original_pid);

    if (kill(original_pid, SIGKILL) == -1)
    {
        perror("kill");
        terminate_workers();
        reap_workers();
        return 1;
    }

    int status;
    pid_t result;

    do
    {
        result = waitpid(original_pid, &status, 0);
    }
    while (result == -1 && errno == EINTR);

    if (result != original_pid)
    {
        perror("waitpid");
        terminate_workers();
        reap_workers();
        return 1;
    }

    printf("Worker 1 reaped externally.\n");

    check_terminated_workers();

    workers = get_workers();

    if (workers[0].pid == 0 &&
        workers[0].is_running == 0 &&
        workers[0].restart_count == 0)
    {
        printf("SUCCESS: ECHILD handled correctly!\n");

        terminate_workers();
        reap_workers();
        return 0;
    }

    printf("FAILED: Worker state was not cleared!\n");

    terminate_workers();
    reap_workers();

    return 1;
}
