#define _POSIX_C_SOURCE 200809L

#include "signal_manager.h"
#include <signal.h>
#include <string.h>
#include <sys/time.h>

/* Global signal flags */
volatile sig_atomic_t shutdown_flag = 0;
volatile sig_atomic_t child_flag = 0;
volatile sig_atomic_t alarm_flag = 0;
volatile sig_atomic_t status_flag = 0;

/* Signal handler */
static void signal_handler(int signo)
{
    switch (signo) {
        case SIGINT:
        case SIGTERM:
            shutdown_flag = 1;
            break;

        case SIGCHLD:
            child_flag = 1;
            break;

        case SIGALRM:
            alarm_flag = 1;
            break;

        case SIGUSR1:
            status_flag = 1;
            break;

        default:
            break;
    }
}

/* Install signal handlers */
int setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    const int signals[] = {
        SIGINT,
        SIGTERM,
        SIGCHLD,
        SIGALRM,
        SIGUSR1
    };

    size_t count = sizeof(signals) / sizeof(signals[0]);

    for (size_t i = 0; i < count; i++) {
        if (sigaction(signals[i], &sa, NULL) == -1) {
            return -1;
        }
    }

    return 0;
}

/* Start periodic timer */
int start_periodic_timer(void)
{
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));

    timer.it_value.tv_sec = 1;
    timer.it_interval.tv_sec = 1;

    return setitimer(ITIMER_REAL, &timer, NULL);
}

/* Stop periodic timer */
int stop_periodic_timer(void)
{
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));

    return setitimer(ITIMER_REAL, &timer, NULL);
}
