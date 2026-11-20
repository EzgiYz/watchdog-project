#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("TEST: Graceful Shutdown\n");

    if (start_workers() != 0)
    {
        printf("FAILED: Workers could not start!\n");
        return 1;
    }

    sleep(2);

    printf("\nRequesting shutdown...\n");

    terminate_workers();
    reap_workers();

    const Worker *workers = get_workers();

    int success = 1;

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        printf("Worker %d: running=%d, pid=%ld, restarts=%d\n",
               i + 1,
               workers[i].is_running,
               (long)workers[i].pid,
               workers[i].restart_count);

        if (workers[i].is_running || workers[i].pid != 0)
            success = 0;
    }

    check_terminated_workers();

    if (success)
    {
        printf("\nSUCCESS: Graceful shutdown works!\n");
        return 0;
    }

    printf("\nFAILED: Some workers are still running!\n");
    return 1;
}
