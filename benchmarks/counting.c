/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
 *
 * Sample MPI "hello world" application in C
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpi.h"

static int limit = 120; /* default */

int main(int argc, char* argv[])
{
    int rank, size, i;

    if (argc > 1) {
        limit = (i = atoi(argv[1])) > 0 ? i : limit;
    }
  
    printf("Counting demo starting with pid %d\n", (int)getpid());

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    for (i = 0; i < limit; ++i) {
        //printf("[%d of %d] Count = %d\n", rank, size, i);
        sleep(2);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();

    return 0;
}
