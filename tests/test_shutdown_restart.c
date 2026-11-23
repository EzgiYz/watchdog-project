#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(void)
{
    printf("TEST: Shutdown Restart Prevention\n");

    if (start_workers() != 0)
        return 1;

    sleep(1);

    request_process_shutdown();

    const Worker *workers = get_workers();

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (workers[i].is_running)
            kill(workers[i].pid, SIGTERM);
    }

    sleep(1);

    // Terminated workers must NOT restart.
    check_terminated_workers();

    workers = get_workers();

    int success = 1;

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        printf("Worker %d: running=%d, pid=%ld, restarts=%d\n",
               i + 1,
               workers[i].is_running,
               (long)workers[i].pid,
               workers[i].restart_count);

        if (workers[i].is_running ||
            workers[i].pid != 0 ||
            workers[i].restart_count != 0)
        {
            success = 0;
        }
    }

    if (success)
    {
        printf("\nSUCCESS: No workers restarted during shutdown!\n");
        return 0;
    }

    printf("\nFAILED: Unexpected restart during shutdown!\n");
    terminate_workers();
    reap_workers();
    return 1;
}
