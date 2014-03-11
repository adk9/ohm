// Copyright (c) 2010-2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ohmd.h"

// List of "active" probes
probe_t     *probes_list;

// allocate a new probe
probe_t *
new_probe(variable_t *var, bool active) {
    probe_t *p;
    if (!var)
        return NULL;

    p = malloc(sizeof(*p));
    if (!p)
        return NULL;

    p->var = var;
    // we allocate a temporary buffer for the probe data here
    if (!var->type)
        return NULL;

    p->buf = calloc(basetype_get_size(var->type), basetype_get_nelem(var->type));
    if (!p->buf) {
        free(p);
        return NULL;
    }
    p->status = active;
    p->next = NULL;
    return p;
}

// add a probe to the probes table
int
probes_list_add(probe_t **table, probe_t *probe)
{
    probe_t *p;

    if (!probe)
        return -1;

    if (!*table) {
        *table = probe;
        return 0;
    }

    p = *table;
    while (p->next)
        p = p->next;

    p->next = probe;
    return 0;
}

//TODO: probes_list_remove

// print all the active probes
void
print_probes(probe_t *probe)
{
    while (probe) {
        if (probe->var) {
            if (is_addr(probe->var->loctype))
                ddebug("%s(0x%lx)\t[GLOBAL]", probe->var->name,
                       probe->var->addr);
            else
                ddebug("%s(%ld)\t[STACK]", probe->var->name,
                       probe->var->offset);
        }
        probe = probe->next;
    }
}

