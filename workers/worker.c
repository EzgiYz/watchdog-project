#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("Worker started! PID: %ld\n", (long)getpid());
    fflush(stdout);

    while (1)
    {
        sleep(1);
    }

    return 0;
}
