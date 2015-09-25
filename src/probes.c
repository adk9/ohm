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
new_probe(char *name, variable_t *var, bool active, bool deref) {
    probe_t *p;
    if (!var)
        return NULL;

    p = malloc(sizeof(*p));
    if (!p) {
        ddebug("unable to allocate memory. skipping probe %s...", name);
        return NULL;
    }

    if (name != NULL) {
        strncpy(p->name, name, strlen(name)+1);
    }
    p->var = var;
    // we allocate a temporary buffer for the probe data here
    if (!var->type) {
        ddebug("invalid type. skipping probe %s...", name);
        return NULL;
    }

    p->buf = calloc(get_type_size(var->type), 1);
    if (!p->buf) {
        ddebug("could not figure out the type size. skipping probe %s...", name);
        free(p);
        return NULL;
    }
    p->status = active;
    p->deref = deref;
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
                ddebug("%s(0x%lx)\t[GLOBAL] %s", probe->name,
                       probe->var->addr, probe->deref ? "deref" : "");
            else
                ddebug("%s(%ld)\t[STACK] %s", probe->name,
                       probe->var->offset, probe->deref ? "deref" : "");
        }
        probe = probe->next;
    }
}

