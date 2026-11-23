#include "process_manager.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

static Worker workers[MAX_WORKERS];

static int shutting_down = 0;

void request_process_shutdown(void)
{
    shutting_down = 1;
}

int start_worker(int worker_id)
{
    if (worker_id < 0 || worker_id >= MAX_WORKERS ||
        shutting_down)
        return -1;

    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        return -1;
    }

    int flags = fcntl(pipefd[1], F_GETFD);

    if (flags == -1 ||
        fcntl(pipefd[1], F_SETFD, flags | FD_CLOEXEC) == -1)
    {
        perror("fcntl failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(pipefd[0]);

        execl("./worker", "worker", (char *)NULL);

        int exec_error = errno;
        (void)write(pipefd[1], &exec_error, sizeof(exec_error));
        _exit(127);
    }

    close(pipefd[1]);

    int exec_error = 0;
    ssize_t result;

    do
    {
        result = read(pipefd[0], &exec_error, sizeof(exec_error));
    }
    while (result == -1 && errno == EINTR);

    close(pipefd[0]);

    if (result != 0)
    {
        if (result > 0)
        {
            errno = exec_error;
            perror("exec failed");
        }
        else
        {
            perror("exec status read failed");
        }

        waitpid(pid, NULL, 0);
        return -1;
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
    shutting_down = 0;

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        workers[i].pid = 0;
        workers[i].worker_id = i;
        workers[i].restart_count = 0;
        workers[i].is_running = 0;
    }

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (start_worker(i) != 0)
        {
            fprintf(stderr,
                    "Failed to start Worker %d\n", i + 1);

            terminate_workers();
            reap_workers();

            return -1;
        }
    }

    return 0;
}

const Worker *get_workers(void)
{
    return workers;
}

void check_terminated_workers(void)
{
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (!workers[i].is_running || workers[i].pid <= 0)
            continue;

        int status;
        pid_t pid;

        do
        {
            pid = waitpid(workers[i].pid, &status, WNOHANG);
        }
        while (pid == -1 && errno == EINTR);

        if (pid == 0)
            continue;

        if (pid == -1)
{
    if (errno == ECHILD)
    {
        fprintf(stderr,
                "Worker %d is no longer a waitable child\n",
                i + 1);

        workers[i].pid = 0;
        workers[i].is_running = 0;
    }
    else
    {
        perror("waitpid failed");
    }

    continue;
}

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
            !shutting_down &&
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
        else if (unexpected && !shutting_down)
        {
            printf("Worker %d restart limit reached!\n",
                   i + 1);
        }
    }
}
void terminate_workers(void)
{
    request_process_shutdown();

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (workers[i].is_running && workers[i].pid > 0)
        {
            kill(workers[i].pid, SIGTERM);
        }
    }
}

void reap_workers(void)
{
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (workers[i].pid > 0)
        {
            waitpid(workers[i].pid, NULL, 0);
            workers[i].pid = 0;
            workers[i].is_running = 0;
        }
    }
}
