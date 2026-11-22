
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "signal_manager.h"
#include "process_manager.h"

/* Worker durumlarini ekrana yazdirir */
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

/* Ana program */
int main(void)
{
    sigset_t blocked_signals;
    sigset_t previous_mask;
    int result = EXIT_SUCCESS;

    printf("Watchdog starting...\n");

    /* 1. Signal handler'larini kur */
    if (setup_signal_handlers() == -1) {
        perror("setup_signal_handlers");
        return EXIT_FAILURE;
    }

    /* 2. Process Manager modulu ile worker baslat */
    if (start_workers() != 0) {
        fprintf(stderr, "Failed to start workers.\n");
        request_process_shutdown();
        terminate_workers();
        reap_workers();
        return EXIT_FAILURE;
    }

    /* 3. Sinyalleri ana dongu icin bloke et */
    sigemptyset(&blocked_signals);

    sigaddset(&blocked_signals, SIGCHLD);
    sigaddset(&blocked_signals, SIGALRM);
    sigaddset(&blocked_signals, SIGUSR1);
    sigaddset(&blocked_signals, SIGINT);
    sigaddset(&blocked_signals, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &blocked_signals,
                    &previous_mask) == -1) {
        perror("sigprocmask");
        result = EXIT_FAILURE;
        goto shutdown;
    }

    /* 4. Her saniye periyodik kontrol baslat */
    if (start_periodic_timer() == -1) {
        perror("start_periodic_timer");
        result = EXIT_FAILURE;
        goto restore_mask;
    }

    printf("Watchdog running. PID: %ld\n", (long)getpid());
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    /* 5. Ana kontrol dongusu */
    while (!shutdown_flag) {

        /* SIGCHLD veya SIGALRM gelirse worker kontrol et */
        if (child_flag || alarm_flag) {
            child_flag = 0;
            alarm_flag = 0;

            if (!shutdown_flag) {
                check_terminated_workers();
            }
        }

        /* SIGUSR1 gelirse durum raporu olustur */
        if (status_flag) {
            status_flag = 0;
            print_worker_status();
        }

        /* Kapanis istenirse donguden cik */
        if (shutdown_flag) {
            break;
        }

        /* CPU tuketmeden yeni sinyali bekle */
        sigsuspend(&previous_mask);
    }

    /* 6. Zamanlayiciyi durdur */
    if (stop_periodic_timer() == -1) {
        perror("stop_periodic_timer");
        result = EXIT_FAILURE;
    }

restore_mask:
    /* Onceki sinyal maskesini geri yukle */
    if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
        perror("restore signal mask");
        result = EXIT_FAILURE;
    }

shutdown:
    /* 7. Guvenli kapanis */
    printf("\nShutdown requested...\n");
    fflush(stdout);

    /* Kapanis sirasinda restart yapilmasin */
    request_process_shutdown();

    /* Calisan worker'lara SIGTERM gonder */
    terminate_workers();

    /* Sonlanan child process'leri topla */
    reap_workers();

    printf("Watchdog stopped.\n");

    return result;
}
