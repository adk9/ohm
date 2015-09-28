#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpi.h"

static int limit = 120; /* default */
int count;

int main(int argc, char* argv[])
{
    int init, finalized;
    int rank, size;

    if (argc > 1) {
        limit = (count = atoi(argv[1])) > 0 ? count : limit;
    }
  
    printf("Counting demo starting with pid %d\n", (int)getpid());

    MPI_Initialized(&init);
    if (!init)
        MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    for (count = 0; count < limit; ++count) {
        printf("[%d of %d] Count = %d\n", rank, size, count);
        sleep(2);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalized(&finalized);
    if (!finalized)
        MPI_Finalize();

    return 0;
}
