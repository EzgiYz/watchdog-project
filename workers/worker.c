#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t running = 1;

/* Handle termination signal */
static void handle_sigterm(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    /* Register SIGTERM handler */
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

    /* Unblock SIGTERM inherited from the parent process */
    sigset_t signals;

    sigemptyset(&signals);
    sigaddset(&signals, SIGTERM);

    if (sigprocmask(SIG_UNBLOCK, &signals, NULL) == -1) {
        perror("sigprocmask failed");
        return 1;
    }

    printf("Worker started! PID: %ld\n", (long)getpid());
    fflush(stdout);

    while (running) {
        sleep(1);
    }

    printf("Worker stopped normally. PID: %ld\n",
           (long)getpid());

    return 0;
}