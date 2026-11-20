#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

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

    const Worker *workers = get_workers();
    pid_t original_pid = workers[0].pid;

    printf("\nOriginal Worker 1 PID: %ld\n",
           (long)original_pid);

    fflush(stdout);

    sleep(1);

    printf("Killing Worker 1...\n");

    if (kill(original_pid, SIGKILL) == -1)
    {
        perror("kill");
        terminate_workers();
        reap_workers();
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return 1;
    }

    alarm(1);

    int success = 0;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        while (!check_requested)
        {
            sigsuspend(&old_mask);
        }

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

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    if (!success)
    {
        printf("FAILED: Worker was not restarted!\n");
        return 1;
    }

    return 0;
}
