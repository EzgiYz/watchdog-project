#include "process_manager.h"

#include <stdio.h>

int main(void)
{
    printf("TEST: exec() failure detection\n");

    int result = start_workers();

    if (result == -1)
    {
        printf("SUCCESS: Worker startup failure detected!\n");
        return 0;
    }

    printf("FAILED: Missing worker executable was not detected!\n");

    terminate_workers();
    reap_workers();

    return 1;
}
