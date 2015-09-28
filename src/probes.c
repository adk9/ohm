// Copyright (c) 2010-2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
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
static regex_t probe_re_structmem;

static inline bool
_str_is_alnum(char *s) {
    while(isalnum(*s)) ++s;
    return (*s == 0);
}

static inline bool
_str_is_digit(char *s) {
    while(isdigit(*s)) ++s;
    return (*s == 0);
}

// initialize the probe infrastructure
int probe_initialize(void) {
    int ret = regcomp(&probe_re_arrind, "([[:alnum:]]+)\\[([[:alnum:]]*):?([[:alnum:]]*)\\]", REG_EXTENDED);
    if (ret) {
        derror("failed to compile probe_re_arrind regex.");
        return -1;
    }

    ret = regcomp(&probe_re_structmem, "([[:alnum:]]+)->([[:alnum:]]+)", REG_EXTENDED);
    if (ret) {
        derror("failed to compile probe_re_structmem regex.");
        return -1;
    }
    return 1;
}

// finalize the probe infrastructure
void probe_finalize(void) {
    regfree(&probe_re_arrind);
    regfree(&probe_re_structmem);
}

/// Given the name of the probe, @p name, this function determines the
/// type of the probe it is, returns the name of the variable that we
/// should be probing, any referenced vars; and initializes the
/// following probe parameters: the type of the probe and optionally
/// the start and the number of array elements to probe.
static int _set_probe_type(probe_t *p, char *name, char **pvar, char **ref) {
    p->type = 0;
    p->start = 0;
    p->num = -1;
    p->lower = NULL;
    p->upper = NULL;

    char *pname = strchr(name, '*');
    if (pname != NULL) {
        p->type = OHM_DEREF;
        *pvar = strdup(pname+1);
        return 1;
    }

    pname = strchr(name, '&');
    if (pname != NULL) {
        p->type = OHM_PTR_ADDR;
        *pvar = strdup(pname+1);
        return 1;
    }

    regmatch_t matches[4];
    int ret = regexec(&probe_re_arrind, name, 4, matches, 0);
    if (!ret) {
        p->type = OHM_ARR_IND;
        *(name+matches[1].rm_eo) = 0;
        *pvar = strdup(name+matches[1].rm_so);

        // if a lower index is specified...
        if (matches[2].rm_so != matches[2].rm_eo) {
            *(name+matches[2].rm_eo) = 0;
            if (_str_is_digit(name+matches[2].rm_so)) {
                p->start = atoi(name+matches[2].rm_so);
                p->lower = NULL;
            } else {
                p->lower = get_variable(name+matches[2].rm_so);
                p->start = 0;
            }
        } else {
            p->start = 0;
            p->lower = NULL;
        }

        if (matches[3].rm_so != matches[3].rm_eo) {
            *(name+matches[3].rm_eo) = 0;
            if (_str_is_digit(name+matches[3].rm_so)) {
                p->num = atoi(name+matches[3].rm_so)-(p->start);
                p->upper = NULL;
            } else {
                p->upper = get_variable(name+matches[3].rm_so);
                p->num = 0;
            }
        } else {
            if (matches[3].rm_so != matches[2].rm_eo+1) {
                p->num = 0;
            }
        }
        return 1;
    }

    ret = regexec(&probe_re_structmem, name, 3, matches, 0);
    if (!ret) {
        p->type = OHM_STRUCT_MEM;
        *(name+matches[1].rm_eo) = 0;
        *pvar = strdup(name+matches[1].rm_so);

        *(name+matches[2].rm_eo) = 0;
        *ref = strdup(name+matches[2].rm_so);
        return 1;
    }

    *pvar = strdup(name);
    return 1;
}

// activate a probe and allocate a buffer for it given the following
// arguments:
//
// p: probe pointer
// var: the variable corresponding to the probe
// active: if the probe is active or not
static int _activate_probe(probe_t *p, variable_t *var, bool active, char *ref) {
    if (!var)
        return -1;

    p->var = var;
    // we allocate a temporary buffer for the probe data here
    if (!var->type) {
        ddebug("invalid type. Skipping probe %s...", p->name);
        return -1;
    }

    p->buf = calloc(get_type_size(var->type), 1);
    if (!p->buf) {
        ddebug("could not figure out the type size. Skipping probe %s...", p->name);
        return -1;
    }

    if (is_struct_mem(p->type)) {
        p->start = 0;
        basetype_t *type = get_type_ptr(var->type);
        if (is_struct(type->ohm_type)) {
            int i;
            for (i = 0; i < get_type_nelem(type); ++i) {
                if (!strcmp(ref, type->elems[i]->name)) {
                    p->num = type->elems[i]->size;
                    break;
                }
                p->start += type->elems[i]->size;
            }
        }
    }
    
    p->status = active;
    p->next = NULL;
    return 1;
}


// allocate a new probe
probe_t *
new_probe(char *name) {
    char *pname = 0;
    char *ref = 0;

    probe_t *p;
    p = calloc(1, sizeof(*p));
    if (!p) {
        ddebug("unable to allocate memory. Skipping probe %s...", name);
        return NULL;
    }

    strcpy(p->name, name);
    char *name_ = strdup(name);
    _set_probe_type(p, name_, &pname, &ref);
    free(name_);

    variable_t *v = get_variable(pname);
    if (!v) {
        // if it is not a variable, check whether a function probe
        // is requested.
        function_t *f = get_function(pname);
        if (!f) {
            ddebug("Skipping non-existent probe %s.", pname);
            free(p);
            free(pname);
            if (ref)
                free(ref);
            return NULL;
        }
    }
    if (_activate_probe(p, v, 1, ref) < 0) {
        free(p);
        free(pname);
        if (ref)
            free(ref);
        return NULL;
    }

    free(pname);
    if (ref)
        free(ref);
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
                ddebug("%s(0x%lx)\t[GLOBAL]", probe->name,
                       probe->var->addr);
            else
                ddebug("%s(%ld)\t[STACK]", probe->name,
                       probe->var->offset);
        }
        probe = probe->next;
    }
}

