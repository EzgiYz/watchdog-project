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
    {
        printf("FAILED: Workers could not start!\n");
        return 1;
    }

    printf("Periodic monitoring started.\n");
    fflush(stdout);

    alarm(1);

    int checks = 0;

    while (checks < 5)
    {
        pause();

        if (check_requested)
        {
            check_requested = 0;

            printf("Periodic worker check %d\n", checks + 1);
            check_terminated_workers();

            checks++;

            alarm(1);
        }
    }

    printf("Stopping workers...\n");

    terminate_workers();
    reap_workers();

    printf("SUCCESS: Periodic monitoring completed!\n");

    return 0;
}
