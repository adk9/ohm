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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define DEFAULT_OHMFILE         "default.ohm"
#define DEFAULT_INTERVAL        3.0

typedef unsigned long addr_t;

/**********************************************************************/

/* Types (base types and aggregate types) */

#define OHM_MAX_NUM_TYPES       2048

// OHM types.
#define OHM_TYPE_UNSIGNED  (1<<0)
#define OHM_TYPE_ARRAY     (1<<1)
#define OHM_TYPE_STRUCT    (1<<2)
#define OHM_TYPE_INT       (1<<3)
#define OHM_TYPE_UINT      (1<<3)+OHM_TYPE_UNSIGNED
#define OHM_TYPE_DOUBLE    (1<<4)
#define OHM_TYPE_FLOAT     (1<<5)
#define OHM_TYPE_CHAR      (1<<6)
#define OHM_TYPE_UCHAR     (1<<6)+OHM_TYPE_UNSIGNED
#define OHM_TYPE_LONG      (1<<7)
#define OHM_TYPE_ULONG     (1<<7)+OHM_TYPE_UNSIGNED
#define OHM_TYPE_LONG_INT  (1<<8)
#define OHM_TYPE_LONG_UINT (1<<8)+OHM_TYPE_UNSIGNED
#define OHM_TYPE_PTR       (1<<9)
#define OHM_TYPE_ALIAS     (1<<10)

// Convenience macros to determine OHM type.
#define is_alias(v)    ((v) & OHM_TYPE_ALIAS)
#define is_array(v)    ((v) & OHM_TYPE_ARRAY)
#define is_struct(v)   ((v) & OHM_TYPE_STRUCT)
#define is_unsigned(v) ((v) & OHM_TYPE_UNSIGNED)
#define is_scalar(v)   ((v) >= OHM_TYPE_INT)
#define is_ptr(v)      ((v) & OHM_TYPE_PTR)

typedef struct basetype_t basetype_t;
struct basetype_t
{
    unsigned int id;
    short        ohm_type;
    char         name[128];
    size_t       size;
    unsigned int nelem;
    basetype_t **elems;
};

extern basetype_t   types_table[OHM_MAX_NUM_TYPES];
extern unsigned int types_table_size;

basetype_t* get_type(int id);
basetype_t* get_or_add_type(int id);
inline size_t get_type_size(basetype_t *type);
inline unsigned int get_type_nelem(basetype_t *type);
inline basetype_t* get_type_alias(basetype_t *type);
inline basetype_t* get_type_ptr(basetype_t *type);
inline int get_type_ohmtype(basetype_t *type);
int add_basetype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die);
int add_complextype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die);

/**********************************************************************/

/* Functions */

#define OHM_MAX_NUM_FUNCTIONS   4096

typedef struct function_t function_t;
struct function_t
{
    char	name[128];
    addr_t	lowpc;
    addr_t	hipc;
};

extern function_t   fns_table[OHM_MAX_NUM_FUNCTIONS];
extern unsigned int fns_table_size;

extern function_t  *main_fn;

function_t* get_function(char *name);
void refresh_compound_sizes(void);
inline int in_function(function_t *f, unsigned long ip);
inline int in_main(unsigned long ip);
void print_all_functions(void);

/**********************************************************************/

/* Variable Locations */

// The location type of a variable.
#define OHM_ADDRESS  (1<<0)
#define   OHM_FBREG  (1<<1)
#define     OHM_REG  (1<<2)
#define OHM_LITERAL  (1<<3)

// Convenience macros to determine the location type
// of the variable.
#define    is_addr(v) ((v) & OHM_ADDRESS)
#define   is_fbreg(v) ((v) & OHM_FBREG)
#define     is_reg(v) ((v) & OHM_REG)
#define is_literal(v) ((v) & OHM_LITERAL)

/* Variables */

#define OHM_MAX_NUM_VARS        4096

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

extern variable_t   vars_table[OHM_MAX_NUM_VARS];
extern unsigned int vars_table_size;

variable_t* get_variable(char *name);
int add_var_location(variable_t *var, Dwarf_Debug dbg, Dwarf_Die die,
                     Dwarf_Attribute attr, Dwarf_Half form);
int add_var_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die child_die);
void print_all_variables(void);

/**********************************************************************/

/* Probes */

// The type of the probe.
#define    OHM_DEREF   (1<<1) // dereference, e.g. *x
#define OHM_PTR_ADDR   (1<<2) // address of a pointer, e.g. &x
#define  OHM_ARR_IND   (1<<3) // array index, e.g. x[5]
#define OHM_STRUCT_MEM (1<<4) // member of a struct ptr, e.g. x->y

#define    is_deref(v)    ((v) & OHM_DEREF)
#define is_ptr_addr(v)    ((v) & OHM_PTR_ADDR)
#define  is_arr_ind(v)    ((v) & OHM_ARR_IND)
#define  is_struct_mem(v) ((v) & OHM_STRUCT_MEM)

typedef struct probe_t probe_t;
struct probe_t
{
    char        name[256];   // the name of the probe
    variable_t *var;         // the variable.
    char       *buf;         // this is a buffer we read data into.
    bool        status;      // status of the probe.
    int         type;        // type of the probe
    int         start;       // start index for array probes
    int         num;         // number of elements for array probes
    variable_t *lower;       // lower dynamic array index
    variable_t *upper;       // upper dynamic array index
    probe_t    *next;        // linked list of probes.
};

extern probe_t *probes_list;

probe_t* new_probe(char *name);
int probes_list_add(probe_t **table, probe_t *probe);
void print_probes(probe_t *probe);
int probe_initialize(void);
void probe_finalize(void);

/**********************************************************************/

/* DWARF utility functions for ohmd */

// determine whether the given DWARF form is a location
int is_location_form(int form);

// check if the type represented by @die@ is a scalar type or not.
int is_base_type(Dwarf_Die die);

// get the number denoted by a DWARF attribute 'attr'
void get_number(Dwarf_Attribute attr, Dwarf_Unsigned *val);

// get name of the child die
int get_child_name(Dwarf_Debug dbg, Dwarf_Die child, char *name, int size);

// get name of a parent die (this also ensures the parent die is not a
// compile unit
int get_parent_name(Dwarf_Debug dbg, Dwarf_Die parent, char *name, int size);

// get the offset and type ID of a die
int get_offset_tid(Dwarf_Die die, Dwarf_Off *offset, Dwarf_Unsigned *tid);

// get the byte offsets (locations) of struct members
int get_member_location(Dwarf_Die die, Dwarf_Unsigned *loc);

// Callback function that represents the DWARF query operation.
typedef int (*dwarf_query_cb_t)(Dwarf_Debug, Dwarf_Die, Dwarf_Die);

// traverse a die and all of its children while invoking a callback
// function on each child die.
int traverse_die(dwarf_query_cb_t cb, Dwarf_Debug dbg, Dwarf_Die parent_die,
                 Dwarf_Die child_die);

/**********************************************************************/

/* Lua utility functions. */

inline void lua_pushbuf(lua_State *L, basetype_t *type, void *val);

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

/* XPMEM support */

#if HAVE_XPMEM

#define OHM_MAX_XPMEM_SEGS  128

// Single-copy from a remote processes's memory
int xpmem_copy(void *dst, void *src, size_t size);

// Attach to a remote XPMEM segment mapping it into our address space.
int xpmem_attach_mem(pid_t pid);

// Detach from a remote memory segment
void xpmem_detach_mem(void);

#endif

/**********************************************************************/

#endif /* _OHMD_H */
