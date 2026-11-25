#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>

#include "signal_manager.h"
#include "process_manager.h"
#include "logger.h"

/* Report worker status */
static void print_worker_status(void)
{
    const Worker *workers = get_workers();

    if (workers == NULL) {
        fprintf(stderr, "Worker information unavailable.\n");
        logger_log(LOG_ERROR, "Worker information unavailable.");
        return;
    }

    printf("\n========== WATCHDOG STATUS ==========\n");

    for (int i = 0; i < MAX_WORKERS; i++) {
        printf("Worker %d | PID: %ld | Running: %s | Restarts: %d\n",
               workers[i].worker_id + 1,
               (long)workers[i].pid,
               workers[i].is_running ? "YES" : "NO",
               workers[i].restart_count);
    }

    printf("=====================================\n");
    fflush(stdout);

    logger_status();
}

/* Log worker termination during shutdown */
static void log_worker_termination(int worker_id,
                                   pid_t pid,
                                   int status)
{
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        logger_log(
            exit_code == 0 ? LOG_INFO : LOG_WARN,
            "Worker %d (PID %ld) exited with code %d",
            worker_id + 1,
            (long)pid,
            exit_code
        );
    }
    else if (WIFSIGNALED(status)) {
        logger_log(LOG_WARN,
                   "Worker %d (PID %ld) terminated by signal %d",
                   worker_id + 1,
                   (long)pid,
                   WTERMSIG(status));
    }
}

/* Graceful shutdown */
static void shutdown_workers_safely(void)
{
    printf("\nShutdown requested...\n");
    fflush(stdout);

    logger_log(LOG_INFO, "Shutdown requested.");

    /* Prevent worker restarts during shutdown */
    request_process_shutdown();

    /* Stop periodic timer */
    if (stop_periodic_timer() == -1) {
        perror("stop_periodic_timer");
        logger_log(LOG_ERROR, "Failed to stop periodic timer.");
    }

    /* Send SIGTERM to workers */
    terminate_workers();

    /* Wait up to 3 seconds for workers to terminate */
    struct timespec delay = {0, 100000000L};

    for (int attempt = 0; attempt < 30; attempt++) {
        int all_finished = 1;
        const Worker *workers = get_workers();

        for (int i = 0; i < MAX_WORKERS; i++) {
            pid_t pid = workers[i].pid;

            if (pid <= 0) {
                continue;
            }

            int status;
            pid_t result;

            do {
                result = waitpid(pid, &status, WNOHANG);
            } while (result == -1 && errno == EINTR);

            if (result > 0) {
                log_worker_termination(i, result, status);
            }
            else if (result == 0) {
                all_finished = 0;
            }
            else if (result == -1 && errno != ECHILD) {
                perror("waitpid");

                logger_log(LOG_ERROR,
                           "waitpid failed for PID %ld",
                           (long)pid);

                all_finished = 0;
            }
        }

        if (all_finished) {
            break;
        }

        struct timespec remaining = delay;

        while (nanosleep(&remaining, &remaining) == -1) {
            if (errno != EINTR) {
                break;
            }
        }
    }

    /* Force stop workers that are still running */
    const Worker *workers = get_workers();

    for (int i = 0; i < MAX_WORKERS; i++) {
        pid_t pid = workers[i].pid;

        if (pid <= 0) {
            continue;
        }

        int status;
        pid_t result;

        do {
            result = waitpid(pid, &status, WNOHANG);
        } while (result == -1 && errno == EINTR);

        if (result > 0) {
            log_worker_termination(i, result, status);
        }
        else if (result == 0) {
            printf("Force stopping worker PID %ld\n", (long)pid);

            logger_log(LOG_WARN,
                       "Force stopping worker PID %ld",
                       (long)pid);

            if (kill(pid, SIGKILL) == -1 && errno != ESRCH) {
                perror("kill SIGKILL");

                logger_log(LOG_ERROR,
                           "Failed to force stop PID %ld",
                           (long)pid);
            }
        }
        else if (result == -1 && errno != ECHILD) {
            perror("waitpid");

            logger_log(LOG_ERROR,
                       "waitpid failed for PID %ld",
                       (long)pid);
        }
    }

    reap_workers();

    printf("Watchdog stopped.\n");
    fflush(stdout);

    logger_log(LOG_INFO, "Watchdog stopped.");
}

int main(void)
{
    sigset_t blocked_signals;
    sigset_t previous_mask;
    int result = EXIT_SUCCESS;

    /* Initialize logger */
    if (logger_init("watchdog.log") == -1) {
        fprintf(stderr, "Failed to initialize logger.\n");
        return EXIT_FAILURE;
    }

    printf("Watchdog starting...\n");
    fflush(stdout);

    logger_log(LOG_INFO, "Watchdog starting...");

    /* 1. Setup signal handlers */
    if (setup_signal_handlers() == -1) {
        perror("setup_signal_handlers");

        logger_log(LOG_ERROR, "Failed to setup signal handlers.");
        logger_close();

        return EXIT_FAILURE;
    }

    /* 2. Start workers */
    if (start_workers() != 0) {
        fprintf(stderr, "Failed to start workers.\n");

        logger_log(LOG_ERROR, "Failed to start workers.");

        shutdown_workers_safely();
        logger_close();

        return EXIT_FAILURE;
    }

    /* 3. Block signals managed by the main loop */
    sigemptyset(&blocked_signals);

    sigaddset(&blocked_signals, SIGCHLD);
    sigaddset(&blocked_signals, SIGALRM);
    sigaddset(&blocked_signals, SIGUSR1);
    sigaddset(&blocked_signals, SIGINT);
    sigaddset(&blocked_signals, SIGTERM);

    if (sigprocmask(SIG_BLOCK,
                    &blocked_signals,
                    &previous_mask) == -1) {
        perror("sigprocmask");

        logger_log(LOG_ERROR, "Failed to block signals.");

        shutdown_workers_safely();
        logger_close();

        return EXIT_FAILURE;
    }

    /* 4. Start periodic timer */
    if (start_periodic_timer() == -1) {
        perror("start_periodic_timer");

        logger_log(LOG_ERROR, "Failed to start periodic timer.");

        result = EXIT_FAILURE;
        goto cleanup;
    }

    printf("Watchdog running. PID: %ld\n", (long)getpid());
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    logger_log(LOG_INFO,
               "Watchdog running. PID: %ld",
               (long)getpid());

    /* 5. Main control loop */
    while (!shutdown_flag) {

        if (child_flag || alarm_flag) {
            child_flag = 0;
            alarm_flag = 0;

            if (!shutdown_flag) {
                check_terminated_workers();
            }
        }

        if (status_flag) {
            status_flag = 0;

            logger_log(LOG_INFO, "SIGUSR1 status request received.");

            print_worker_status();
        }

        if (shutdown_flag) {
            break;
        }

        sigsuspend(&previous_mask);
    }

cleanup:

    /* 6. Gracefully stop workers */
    shutdown_workers_safely();

    /* 7. Restore previous signal mask */
    if (sigprocmask(SIG_SETMASK,
                    &previous_mask,
                    NULL) == -1) {
        perror("restore signal mask");

        logger_log(LOG_ERROR, "Failed to restore signal mask.");

        result = EXIT_FAILURE;
    }

    logger_close();

    return result;
}