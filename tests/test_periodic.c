#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

static volatile sig_atomic_t check_requested = 0;

static void alarm_handler(int signo)
{
    (void)signo;
    check_requested = 1;
}

int main(void)
{
    struct sigaction sa = {0};

    sigset_t block_mask;
    sigset_t old_mask;

    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGALRM);

    // Block SIGALRM before checking the flag.
    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1)
    {
        perror("sigprocmask");
        return 1;
    }

    if (start_workers() != 0)
    {
        printf("FAILED: Workers could not start!\n");
        return 1;
    }

    printf("Periodic monitoring started.\n");
    fflush(stdout);

    int checks = 0;

    alarm(1);

    while (checks < 5)
    {
        // Atomically unblock signals and wait.
        while (!check_requested)
        {
            sigsuspend(&old_mask);
        }

        check_requested = 0;

        printf("Periodic worker check %d\n", checks + 1);

        check_terminated_workers();

        checks++;

        if (checks < 5)
            alarm(1);
    }

    alarm(0);

    terminate_workers();
    reap_workers();

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    printf("SUCCESS: Periodic monitoring completed!\n");

    return 0;
}
