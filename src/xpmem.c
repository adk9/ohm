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

typedef struct ohm_seg_t ohm_seg_t;
struct ohm_seg_t {
    xpmem_segid_t seg;
    xpmem_apid_t  ap;
    void*         addr;   /* local virtual address */
    void*         start;  /* remote virtual address */
    unsigned long len;    /* length of the segment */
    unsigned long perm;   /* permissions - octal value */
};

static int       xpmem_nseg; /* number of XPMEM segments */
static ohm_seg_t xpmem_segs[OHM_MAX_XPMEM_SEGS];

int
xpmem_copy(void *dst, void *src, size_t size)
{
    int i;
    unsigned long offset;

     for (i = 0; i < xpmem_nseg; i++) { 
         if ((src >= xpmem_segs[i].start) && 
             (src <= xpmem_segs[i].start+xpmem_segs[i].len-size)) { 
             offset = src-xpmem_segs[i].start; 
             memcpy(dst, xpmem_segs[i].addr+offset, size); 
             return size; 
         } 
     } 
     return -1;
}

int
xpmem_attach_mem(pid_t pid)
{
    FILE *file;
    char buf[2048], name[256];
    unsigned long vm_start;
    unsigned long vm_end;
    char r, w, x, p;
    int n;
    struct xpmem_addr addr;
    ohm_seg_t *seg;

    sprintf(buf, "/proc/%d/maps", pid);
    file = fopen(buf, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    xpmem_nseg = 0;
    while (fgets(buf, sizeof(buf), file) != NULL) {
        n = sscanf(buf, "%lx-%lx %c%c%c%c %*lx %*s %*ld %255s", &vm_start, &vm_end,
                   &r, &w, &x, &p, name);
        if (n < 2) {
            derror("unexpected line: %s\n", buf);
            continue;
        }

        if (strcmp(name, "[heap]") && strcmp(name, "[stack]"))
            continue;

        seg = &xpmem_segs[xpmem_nseg++];
        seg->perm = ((r=='r')*0444)+((w=='w')*0222)+((x=='x')*0111);
        seg->start = (void*)vm_start;
        seg->len = vm_end-vm_start;
        seg->seg = xpmem_make(seg->start, seg->len, XPMEM_PERMIT_MODE,
                              (void*)seg->perm);
        if (seg->seg == -1) {
            perror("xpmem_make");
            return -1;
        }

        seg->ap = xpmem_get(seg->seg, (seg->perm==0444) ? XPMEM_RDONLY : XPMEM_RDWR,
                            XPMEM_PERMIT_MODE, (void*)seg->perm);
        if (seg->ap == -1) {
            perror("xpmem_get");
            return -1;
        }

        addr.apid = seg->ap;
        addr.offset = 0;
        seg->addr = xpmem_attach(addr, seg->len, 0);
        ddebug("Registered segment %p ==> %p, len %lu", seg->addr, seg->start, seg->len);
        if (seg->addr == (void*)-1 || seg->addr == MAP_FAILED) {
            perror("xpmem_attach");
            return -1;
        }
    }
    fclose(file);
    return 0;
}

void
xpmem_detach_mem(void)
{
    int i;
    for (i = 0; i < xpmem_nseg; i++) {
        xpmem_detach(xpmem_segs[i].addr);
        xpmem_release(xpmem_segs[i].ap);
        xpmem_remove(xpmem_segs[i].seg);
    }
}
