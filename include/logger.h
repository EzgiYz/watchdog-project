#ifndef LOGGER_H
#define LOGGER_H

/* Log severity levels */
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/* Initialize the logger and open the log file */
int logger_init(const char *filename);

/* Write a timestamped log message */
void logger_log(LogLevel level, const char *format, ...);

/* Log the current status of all workers */
void logger_status(void);

/* Close the log file and release resources */
void logger_close(void);

#endif