#include "process_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(void)
{
    if (start_workers() != 0)
    {
        printf("Workers could not be started!\n");
        return 1;
    }

    sleep(2);

    const Worker *workers = get_workers();
    pid_t worker_pid = workers[0].pid;

    printf("Terminating Worker 1 (PID: %ld)\n",
           (long)worker_pid);

    kill(worker_pid, SIGKILL);

    sleep(1);

    check_terminated_workers();

    workers = get_workers();

    if (workers[0].is_running == 0)
    {
        printf("SUCCESS: Worker 1 termination detected!\n");
    }
    else
    {
        printf("FAILED: Worker 1 still marked running.\n");
    }

    
    kill(workers[1].pid, SIGTERM);
    sleep(1);
    check_terminated_workers();

    return 0;
}
