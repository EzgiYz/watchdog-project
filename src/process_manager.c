#include "process_manager.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

static Worker workers[MAX_WORKERS];

int start_worker(int worker_id)
{
    if (worker_id < 0 || worker_id >= MAX_WORKERS)
        return -1;

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        return -1;
    }

    if (pid == 0)
    {
        execl("./worker", "worker", (char *)NULL);

        perror("exec failed");
        _exit(127);
    }

    workers[worker_id].pid = pid;
    workers[worker_id].worker_id = worker_id;
    workers[worker_id].is_running = 1;

    printf("Worker %d started with PID %ld\n",
           worker_id + 1, (long)pid);

    return 0;
}

int start_workers(void)
{
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        workers[i].restart_count = 0;

        if (start_worker(i) != 0)
            return -1;
    }

    return 0;
}

const Worker *get_workers(void)
{
    return workers;
}

void check_terminated_workers(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        for (int i = 0; i < MAX_WORKERS; i++)
        {
            if (workers[i].pid != pid)
                continue;

            workers[i].is_running = 0;
            workers[i].pid = 0;

            int unexpected = 0;

            if (WIFEXITED(status))
            {
                int exit_code = WEXITSTATUS(status);

                printf("Worker %d exited with code %d\n",
                       i + 1, exit_code);

                unexpected = (exit_code != 0);
            }
            else if (WIFSIGNALED(status))
            {
                printf("Worker %d terminated by signal %d\n",
                       i + 1, WTERMSIG(status));

                unexpected = 1;
            }

            if (unexpected &&
                workers[i].restart_count < MAX_RESTARTS)
            {
                workers[i].restart_count++;

                printf("Restarting Worker %d (%d/%d)\n",
                       i + 1,
                       workers[i].restart_count,
                       MAX_RESTARTS);

                if (start_worker(i) != 0)
                {
                    fprintf(stderr,
                            "Failed to restart Worker %d\n",
                            i + 1);
                }
            }
            else if (unexpected)
            {
                printf("Worker %d restart limit reached!\n",
                       i + 1);
            }

            break;
        }
     }

 }

