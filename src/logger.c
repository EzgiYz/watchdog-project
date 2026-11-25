#define _POSIX_C_SOURCE 200809L

#include "logger.h"
#include "process_manager.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Log file pointer */
static FILE *log_file = NULL;

/* Convert log level to string */
static const char *level_to_string(LogLevel level)
{
    switch (level) {
        case LOG_INFO:
            return "INFO";

        case LOG_WARN:
            return "WARN";

        case LOG_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}

/* Initialize the logger */
int logger_init(const char *filename)
{
    if (filename == NULL || log_file != NULL) {
        fprintf(stderr, "Invalid logger initialization.\n");
        return -1;
    }

    log_file = fopen(filename, "a");

    if (log_file == NULL) {
        perror("Failed to open log file");
        return -1;
    }

    /* Prevent log descriptor inheritance after exec */
    int fd = fileno(log_file);
    int flags = fcntl(fd, F_GETFD);

    if (flags == -1 ||
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        perror("Failed to set FD_CLOEXEC");

        fclose(log_file);
        log_file = NULL;

        return -1;
    }

    return 0;
}

/* Write a timestamped log message */
void logger_log(LogLevel level, const char *format, ...)
{
    if (log_file == NULL || format == NULL) {
        fprintf(stderr, "Logger is not initialized or message is invalid.\n");
        return;
    }

    time_t now = time(NULL);
    struct tm time_info;
    char timestamp[32];

    if (now == (time_t)-1 ||
        localtime_r(&now, &time_info) == NULL ||
        strftime(timestamp, sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S", &time_info) == 0) {
        fprintf(stderr, "Failed to generate log timestamp.\n");
        return;
    }

    /* Write timestamp and log level */
    if (fprintf(log_file, "[%s] [%s] ",
                timestamp, level_to_string(level)) < 0) {
        perror("Failed to write log header");
        return;
    }

    /* Write formatted message */
    va_list args;
    va_start(args, format);

    int result = vfprintf(log_file, format, args);

    va_end(args);

    if (result < 0) {
        perror("Failed to write log message");
        return;
    }

    /* Finish the log entry */
    if (fprintf(log_file, "\n") < 0) {
        perror("Failed to write newline");
        return;
    }

    /* Flush buffered data */
    if (fflush(log_file) == EOF) {
        perror("Failed to flush log file");
    }
}

/* Log the current status of all workers */
void logger_status(void)
{
    const Worker *workers = get_workers();

    if (workers == NULL) {
        logger_log(LOG_ERROR,
                   "Worker information unavailable.");
        return;
    }

    logger_log(LOG_INFO, "Worker status report:");

    for (int i = 0; i < MAX_WORKERS; i++) {
        logger_log(
            LOG_INFO,
            "Worker %d | PID: %ld | Running: %s | Restarts: %d",
            workers[i].worker_id + 1,
            (long)workers[i].pid,
            workers[i].is_running ? "YES" : "NO",
            workers[i].restart_count
        );
    }
}

/* Close the logger */
void logger_close(void)
{
    if (log_file != NULL) {
        if (fclose(log_file) == EOF) {
            perror("Failed to close log file");
        }

        log_file = NULL;
    }
}