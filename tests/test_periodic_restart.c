#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static volatile sig_atomic_t check_requested = 0;

static void alarm_handler(int signo)
{
    (void)signo;
    check_requested = 1;
}

int main(void)
{
    struct sigaction sa = {0};
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    if (start_workers() != 0)
        return 1;

    const Worker *workers = get_workers();
    pid_t original_pid = workers[0].pid;

    printf("\nOriginal Worker 1 PID: %ld\n",
           (long)original_pid);

    sleep(1);

    printf("Killing Worker 1...\n");
    kill(original_pid, SIGKILL);

    alarm(1);

    int success = 0;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        while (!check_requested)
            pause();

        check_requested = 0;

        printf("Periodic check %d\n", attempt + 1);

        check_terminated_workers();

        workers = get_workers();

        if (workers[0].is_running &&
            workers[0].pid != original_pid &&
            workers[0].restart_count == 1)
        {
            printf("SUCCESS: Worker restarted automatically!\n");
            success = 1;
            break;
        }

        alarm(1);
    }

    alarm(0);

    terminate_workers();
    reap_workers();

    if (!success)
    {
        printf("FAILED: Worker was not restarted!\n");
        return 1;
    }

    return 0;
}
