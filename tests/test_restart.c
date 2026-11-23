#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(void)
{
    if (start_workers() != 0)
        return 1;

    sleep(1);

    for (int attempt = 1; attempt <= 4; attempt++)
    {
        const Worker *workers = get_workers();
        pid_t pid = workers[0].pid;

        printf("\nTEST %d: Killing Worker 1 (PID %ld)\n",
               attempt, (long)pid);

        if (kill(pid, SIGKILL) == -1)
        {
            perror("kill");
            return 1;
        }

        sleep(1);
        check_terminated_workers();

        workers = get_workers();

        printf("Restart count: %d\n",
               workers[0].restart_count);
    }

    const Worker *workers = get_workers();

    if (!workers[0].is_running &&
        workers[0].restart_count == MAX_RESTARTS)
    {
        printf("\nSUCCESS: Restart limit works!\n");
    }
    else
    {
        printf("\nFAILED: Restart limit incorrect!\n");
    }


    if (workers[1].is_running)
    {
        kill(workers[1].pid, SIGTERM);
        waitpid(workers[1].pid, NULL, 0);
    }

    return 0;
}
