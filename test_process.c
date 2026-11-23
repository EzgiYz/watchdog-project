#include "process_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(void)
{
    if (start_workers() != 0)
    {
        printf("Workers could not be started!\n");
        return 1;
    }

    printf("Two workers started successfully!\n");

    sleep(5);

    const Worker *workers = get_workers();

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (workers[i].is_running)
        {
            kill(workers[i].pid, SIGTERM);
            waitpid(workers[i].pid, NULL, 0);
        }
    }

    printf("Test completed. Workers terminated.\n");

    return 0;
}
