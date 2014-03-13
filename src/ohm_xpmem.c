// Copyright (c) 2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if HAVE_XPMEM
#include <xpmem.h>
#endif

#include "ohm_xpmem.h"

static int xpmem_nseg;
static xpmem_segid_t xpmem_segs[XPMEM_MAX_SEGS];

int
ohm_xpmem_init(void)
{
    FILE *file;
    char buf[2048], name[256];
    unsigned long vm_start;
    unsigned long vm_end;
    unsigned long perm;
    char r, w, x, p;
    int n;
    xpmem_segid_t seg;

    file = fopen("/proc/self/maps", "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    xpmem_nseg = 0;
    while (fgets(buf, sizeof(buf), file) != NULL) {
        n = sscanf(buf, "%lx-%lx %c%c%c%c %*lx %*s %*ld %255s", &vm_start, &vm_end,
                   &r, &w, &x, &p, name);
        if (n < 2) {
            fprintf(stderr, "unexpected line: %s\n", buf);
            continue;
        }

        if (strcmp(name, "[heap]") && strcmp(name, "[stack]"))
            continue;

        perm = ((r=='r')*0444)+((w=='w')*0222)+((x=='x')*0111);
        seg = xpmem_make((void*)vm_start, vm_end-vm_start,
                         XPMEM_PERMIT_MODE, (void*)perm);
        if (seg == -1) {
            perror("xpmem_make");
            exit(EXIT_FAILURE);
        }
        xpmem_segs[xpmem_nseg++] = seg;
    }
    fclose(file);
    return 0;
}

void ohm_xpmem_finalize(void)
{
    int i;
    for (i = 0; i < xpmem_nseg; i++)
        xpmem_remove(xpmem_segs[i]);
}
