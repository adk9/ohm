/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int limit = 120; /* default */

int main(int argc, char* argv[])
{
    int i;

    if (argc > 1) {
        limit = (i = atoi(argv[1])) > 0 ? i : limit;
    }
  
    printf("Counting demo starting with pid %d\n", (int)getpid());

    for (i = 0; i < limit; ++i) {
        printf("Count = %d\n", i);
        sleep(2);
    }
    return 0;
}
