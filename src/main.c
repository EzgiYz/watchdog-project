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

/* Worker durumlarini raporla */
static void print_worker_status(void)
{
    const Worker *workers = get_workers();

    if (workers == NULL) {
        fprintf(stderr, "Worker information unavailable.\n");
        return;
    }

    printf("\n========== WATCHDOG STATUS ==========\n");

    for (int i = 0; i < MAX_WORKERS; i++) {
        printf("Worker %d | PID: %ld | Running: %s | Restarts: %d\n",
               workers[i].worker_id,
               (long)workers[i].pid,
               workers[i].is_running ? "YES" : "NO",
               workers[i].restart_count);
    }

    printf("=====================================\n");
    fflush(stdout);
}

/* Guvenli kapanis */
static void shutdown_workers_safely(void)
{
    printf("\nShutdown requested...\n");
    fflush(stdout);

    /* Kapanis sirasinda restart yapilmasin */
    request_process_shutdown();

    /* Periyodik alarmi durdur */
    if (stop_periodic_timer() == -1) {
        perror("stop_periodic_timer");
    }

    /* Worker'lara SIGTERM gonder */
    terminate_workers();

    /*
     * En fazla 3 saniye boyunca child process'lerin
     * sonlanmasini bekle.
     */
    struct timespec delay = {0, 100000000L};

    for (int attempt = 0; attempt < 30; attempt++) {
        int all_finished = 1;
        const Worker *workers = get_workers();

        for (int i = 0; i < MAX_WORKERS; i++) {
            pid_t pid = workers[i].pid;

            if (pid <= 0) {
                continue;
            }

            /*
             * WNOHANG sayesinde bekleme bloke olmaz.
             * Child sonlandiysa waitpid ile toplanir.
             */
            int status;
            pid_t result = waitpid(pid, &status, WNOHANG);

            if (result == 0) {
                all_finished = 0;
            } else if (result == -1 && errno != ECHILD) {
                perror("waitpid");
                all_finished = 0;
            }
        }

        if (all_finished) {
            break;
        }

        /* 100 milisaniye bekle */
        struct timespec remaining = delay;

        while (nanosleep(&remaining, &remaining) == -1) {
            if (errno != EINTR) {
                break;
            }
        }
    }

    /* Hala calisan child process'leri SIGKILL ile durdur */
    const Worker *workers = get_workers();

    for (int i = 0; i < MAX_WORKERS; i++) {
        pid_t pid = workers[i].pid;

        if (pid <= 0) {
            continue;
        }

        /*
         * Child hala calisiyorsa waitpid WNOHANG 0 dondurur.
         */
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == 0) {
            printf("Force stopping worker PID %ld\n", (long)pid);

            if (kill(pid, SIGKILL) == -1 && errno != ESRCH) {
                perror("kill SIGKILL");
            }
        }
    }

    /*
     * Fonksiyon kalan child kayitlarini temizler.
     * Daha once toplanmis child icin ECHILD donmesi normaldir.
     */
    reap_workers();

    printf("Watchdog stopped.\n");
    fflush(stdout);
}

int main(void)
{
    sigset_t blocked_signals;
    sigset_t previous_mask;
    int result = EXIT_SUCCESS;

    printf("Watchdog starting...\n");
    fflush(stdout);

    /* 1. Sinyal handler'larini kur */
    if (setup_signal_handlers() == -1) {
        perror("setup_signal_handlers");
        return EXIT_FAILURE;
    }

    /*
     * 2. Worker'lari sinyalleri bloke etmeden baslat.
     * Boylece child'lar bloklu sinyal maskesini devralmaz.
     */
    if (start_workers() != 0) {
        fprintf(stderr, "Failed to start workers.\n");
        shutdown_workers_safely();
        return EXIT_FAILURE;
    }

    /* 3. Ana dongude yonetilecek sinyalleri bloke et */
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
        shutdown_workers_safely();
        return EXIT_FAILURE;
    }

    /* 4. Periyodik kontrol zamanlayicisini baslat */
    if (start_periodic_timer() == -1) {
        perror("start_periodic_timer");
        result = EXIT_FAILURE;
        goto cleanup;
    }

    printf("Watchdog running. PID: %ld\n", (long)getpid());
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    /* 5. Ana kontrol dongusu */
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
            print_worker_status();
        }

        if (shutdown_flag) {
            break;
        }

        /*
         * Sinyal gelene kadar CPU tuketmeden bekle.
         */
        sigsuspend(&previous_mask);
    }

cleanup:
    /* 6. Worker'lari guvenli sekilde kapat */
    shutdown_workers_safely();

    /* 7. Eski sinyal maskesini geri yukle */
    if (sigprocmask(SIG_SETMASK,
                    &previous_mask,
                    NULL) == -1) {
        perror("restore signal mask");
        result = EXIT_FAILURE;
    }

    return result;
}
