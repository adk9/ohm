// Copyright (c) 2010-2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#ifndef _OHMD_H
#define _OHMD_H

#include <stdio.h>
#include <stdbool.h>

#include <dwarf.h>
#include <libdwarf.h>

#define DEFAULT_OHMFILE         "default.ohm"
#define DEFAULT_INTERVAL        3.0

typedef unsigned long addr_t;

/**********************************************************************/

/* Types (base types and aggregate types) */

#define DEFAULT_NUM_TYPES       256

typedef struct basetype_t basetype_t;
struct basetype_t
{
    unsigned int id;
    char         name[128];
    size_t       size;
    unsigned int nelem;
    basetype_t **elems;
};

extern basetype_t   types_table[DEFAULT_NUM_TYPES];
extern unsigned int types_table_size;

basetype_t* get_type(int id);

inline size_t basetype_get_size(basetype_t *type);

inline unsigned int basetype_get_nelem(basetype_t *type);

int add_basetype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die);

int add_complextype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die);

/**********************************************************************/

/* Functions */

#define DEFAULT_NUM_FUNCTIONS   256

typedef struct function_t function_t;
struct function_t
{
    char	name[128];
    addr_t	lowpc;
    addr_t	hipc;
};

extern function_t   fns_table[DEFAULT_NUM_FUNCTIONS];
extern unsigned int fns_table_size;

extern function_t  *main_fn;

function_t* get_function(char *name);

inline int in_function(function_t *f, unsigned long ip);

inline int in_main(unsigned long ip);

/**********************************************************************/

/* Variable Locations */

// The location type of a variable.
#define OHM_ADDRESS  (1<<0)
#define   OHM_FBREG  (1<<1)
#define     OHM_REG  (1<<2)
#define OHM_LITERAL  (1<<3)

// Convenience macros to determine the location type
// of the variable.
#define    is_addr(v) ((v) & (1<<0))
#define   is_fbreg(v) ((v) & (1<<1))
#define     is_reg(v) ((v) & (1<<2))
#define is_literal(v) ((v) & (1<<3))

/* Variables */

#define DEFAULT_NUM_VARS        256

typedef struct variable_t variable_t;
struct variable_t
{
    char          name[256];
    basetype_t   *type;
    function_t   *function;
    unsigned int  loctype;
  union {
    addr_t        addr;
    signed long   offset;
  };
};

extern variable_t   vars_table[DEFAULT_NUM_VARS];
extern unsigned int vars_table_size;

variable_t* get_variable(char *name);

int add_var_location(variable_t *var, Dwarf_Debug dbg, Dwarf_Die die,
                     Dwarf_Attribute attr, Dwarf_Half form);

int add_var_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die child_die);

void print_all_variables(void);

/**********************************************************************/

/* Probes */

typedef struct probe_t probe_t;
struct probe_t
{
    variable_t *var;
    void       *buf;
    bool        status;
    probe_t    *next;
};

extern probe_t *probes_list;

probe_t* new_probe(variable_t *var, bool active);

int probes_list_add(probe_t **table, probe_t *probe);

void print_probes(probe_t *probe);

/**********************************************************************/

/* DWARF utility functions for ohmd */

// determine whether the given DWARF form is a location
int is_location_form(int form);

// get the number denoted by a DWARF attribute 'attr'
void get_number(Dwarf_Attribute attr, Dwarf_Unsigned *val);

// get name of the child die
int get_child_name(Dwarf_Debug dbg, Dwarf_Die child, char *name, int size);

// get name of a parent die (this also ensures the parent die is not a
// compile unit
int get_parent_name(Dwarf_Debug dbg, Dwarf_Die parent, char *name, int size);

// get the offset and type ID of a die
int get_offset_tid(Dwarf_Die die, Dwarf_Off *offset, Dwarf_Unsigned *tid);

/**********************************************************************/

/* Convenience Macros */

#define OHM_COLOR_ERROR "\x1b[31m"
#define OHM_COLOR_DEBUG "\x1b[32m"
#define OHM_COLOR_RESET "\x1b[0m"

#define derror(format, args...) do {                               \
    fprintf(stderr, OHM_COLOR_ERROR "[ERROR] " OHM_COLOR_RESET     \
            "%s:%d: " format "\n",                                 \
            __func__, __LINE__, ##args); } while (0)

extern int ohm_debug;
#define ddebug(format, args...) do {                               \
    if (ohm_debug)                                                 \
        fprintf(stderr, OHM_COLOR_DEBUG "[DEBUG] " OHM_COLOR_RESET \
                "%s:%d: " format "\n",                             \
                __func__, __LINE__, ##args); } while (0)

#define USED(x) if(x){}else{}
#ifdef __GNUC__
#       if __GNUC__ >= 3
#               undef USED
#               define USED(x) ((void)(x))
#       endif
#endif

/**********************************************************************/

#endif /* _OHMD_H */
