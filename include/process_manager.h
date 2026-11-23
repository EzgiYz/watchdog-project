#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>

#define MAX_WORKERS 2
#define MAX_RESTARTS 3

typedef struct {
    pid_t pid;
    int worker_id;
    int restart_count;
    int is_running;
} Worker;


int start_workers(void);


int start_worker(int worker_id);


void check_terminated_workers(void);


const Worker *get_workers(void);


void terminate_workers(void);


void reap_workers(void);


void request_process_shutdown(void);
#endif
