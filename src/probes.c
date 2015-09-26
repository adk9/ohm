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
#include <sys/types.h>
#include <regex.h>

#include "ohmd.h"

// List of "active" probes
probe_t     *probes_list;

// Regular expressions to parse probe array index descriptions
static regex_t probe_re_arrind;

// initialize the probe infrastructure
int probe_initialize(void) {
    int ret = regcomp(&probe_re_arrind, "([[:alnum:]]+)\\[([[:digit:]]*):?([[:digit:]]*)\\]", REG_EXTENDED);
    if (ret) {
        derror("failed to compile probe_re_arrind regex.");
        return -1;
    }
    return 1;
}

// finalize the probe infrastructure
void probe_finalize(void) {
    regfree(&probe_re_arrind);    
}

// create a probe given the following arguments:
//
// name: probe name
// var: the variable corresponding to the probe
// active: if the probe is active or not
// type: the type of the probe
// start: if the probe is an array index, the starting element
// num: if the probe is an array index, the number of elements to probe
probe_t *_create_probe(char *name, variable_t *var, bool active, int type,
                       int start, int num) {
    probe_t *p;
    if (!var)
        return NULL;

    p = calloc(1, sizeof(*p));
    if (!p) {
        ddebug("unable to allocate memory. Skipping probe %s...", name);
        return NULL;
    }

    if (name != NULL) {
        strncpy(p->name, name, strlen(name)+1);
    }
    p->var = var;
    // we allocate a temporary buffer for the probe data here
    if (!var->type) {
        ddebug("invalid type. Skipping probe %s...", name);
        return NULL;
    }

    p->buf = calloc(get_type_size(var->type), 1);
    if (!p->buf) {
        ddebug("could not figure out the type size. Skipping probe %s...", name);
        free(p);
        return NULL;
    }
    p->status = active;
    if (type < 0 || type > OHM_ARR_IND) {
        ddebug("invalid probe type. Skipping probe %s...", name);
        free(p);
        return NULL;
    }
    p->type = type;
    p->start = start;
    p->num = num;
    p->next = NULL;
    return p;
}

/// Given the name of the probe, @p name, this function determines
/// the type of the probe it is and returns: the name of the variable
/// that we should be attaching to, and the type of the probe and
/// optionally the start and the number of array elements to probe.
static int _get_probe_type(char *name, char **pvar, int *type,
                           int *start, int *num) {
    char *pname = strchr(name, '*');
    if (pname != NULL) {
        *type = OHM_DEREF;
        *pvar = strdup(pname+1);
        return 1;
    }

    pname = strchr(name, '&');
    if (pname != NULL) {
        *type = OHM_PTR_ADDR;
        *pvar = strdup(pname+1);
        return 1;
    }

    regmatch_t matches[4];
    int ret = regexec(&probe_re_arrind, name, 4, matches, 0);
    if (!ret) {
        *type = OHM_ARR_IND;
        *(name+matches[1].rm_eo) = '\0';
        *pvar = strdup(name+matches[1].rm_so);

        if (matches[2].rm_so != matches[2].rm_eo) {
            *(name+matches[2].rm_eo) = '\0';
            *start = atoi(name+matches[2].rm_so);
        } else {
            *start = 0;
        }

        if (matches[3].rm_so != matches[3].rm_eo) {
            *(name+matches[3].rm_eo) = '\0';
            *num = atoi(name+matches[3].rm_so)-(*start);
        } else {
            if (matches[3].rm_so != matches[2].rm_eo+1) {
                *num = 0;
            }
        }
        return 1;
    }

    *pvar = strdup(name);
    return 1;
}

// allocate a new probe
probe_t *
new_probe(char *name) {
    int type = 0;
    int start = 0;
    int num = -1;
    char *pname;
    char *name_ = strdup(name);

    _get_probe_type(name_, &pname, &type, &start, &num);
    variable_t *v = get_variable(pname);
    if (!v) {
        // if it is not a variable, check whether a function probe
        // is requested.
        function_t *f = get_function(pname);
        if (!f) {
            ddebug("Skipping non-existent probe %s.", pname);
            return NULL;
        }
    }
    free(pname);
    free(name_);

    return _create_probe(name, v, 1, type, start, num);
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
                ddebug("%s(0x%lx)\t[GLOBAL]", probe->name,
                       probe->var->addr);
            else
                ddebug("%s(%ld)\t[STACK]", probe->name,
                       probe->var->offset);
        }
        probe = probe->next;
    }
}

