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
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#ifdef __linux__
#include <asm/ptrace.h>
#endif
#include <sys/wait.h>
#include <sys/user.h>
#include <libunwind-ptrace.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <dwarf.h>
#if HAVE_CMA && HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#if HAVE_XPMEM
#include <xpmem.h>
#endif

#include "doctor.h"
#include "dwarf-util.h"

#if (!defined(PTRACE_PEEKUSER) && defined(PTRACE_PEEKUSR))
# define PTRACE_PEEKUSER PTRACE_PEEKUSR
#endif

#if (!defined(PTRACE_POKEUSER) && defined(PTRACE_POKEUSR))
# define PTRACE_POKEUSER PTRACE_POKEUSR
#endif


// Basetype table
static basetype_t   types_table[DEFAULT_NUM_TYPES];
static unsigned int types_table_size;

// Function table
static function_t   fns_table[DEFAULT_NUM_FUNCTIONS];
static unsigned int fns_table_size;

// Variable table
static variable_t   vars_table[DEFAULT_NUM_VARS];
static unsigned int vars_table_size;

// List of "active" probes
static probe_t     *probes_list;

static double       doctor_interval = DEFAULT_INTERVAL;
static int          doctor_debug;
static function_t  *main_fn;
static bool         ohm_shutdown;
static pid_t        ohm_cpid;

// Global Lua state
static lua_State *L;

// Global unwind state
static unw_addr_space_t unw_addrspace;
static unw_cursor_t     unw_cursor;

#if HAVE_XPMEM
typedef struct ohm_seg_t ohm_seg_t;
struct ohm_seg_t {
    xpmem_segid_t seg;
    xpmem_apid_t  ap;
    void*         addr;   /* local virtual address */
    void*         start;  /* remote virtual address */
    unsigned long len;    /* length of the segment */
    unsigned long perm;   /* permissions - octal value */
};

static int xpmem_nseg;    /* number of XPMEM segments */
static ohm_seg_t xpmem_segs[128];
#endif

// Callback function that represents the DWARF query operation.
typedef int (*dwarf_query_cb_t)(Dwarf_Debug, Dwarf_Die, Dwarf_Die);

// Forward Declarations
void ohm_cleanup(int sig);

// fetch a basetype for an object given its id
static basetype_t*
get_type(int id)
{
    int i;
    for (i = 0; i < types_table_size; i++) {
        if (id == types_table[i].id)
            return &types_table[i];
    }
    return NULL;
}

// fetch the function object for a function given its name
static function_t*
get_function(char *name)
{
    int i;
    for (i = 0; i < fns_table_size; i++) {
        if (!strcmp(name, fns_table[i].name))
            return &fns_table[i];
    }
    return NULL;
}

// get a variable object for a variable given its name
static variable_t*
get_variable(char *name)
{
    int i;
    for (i = 0; i < vars_table_size; i++) {
        if (!strcmp(name, vars_table[i].name))
            return &vars_table[i];
    }
    return NULL;
}

static inline size_t
basetype_get_size(basetype_t *type)
{
    if (!type)
        return 0;
    if (type->istypedef)
        return basetype_get_size(get_type(type->nelem));
    else
        return type->size;
}

static inline unsigned int
basetype_get_nelem(basetype_t *type)
{
    if (!type)
        return 0;
    if (type->istypedef)
        return basetype_get_nelem(get_type(type->nelem));
    else
        return type->nelem;
}

static inline void
print_all_variables(void)
{
    int i;
    for (i = 0; i < vars_table_size; i++)
        ddebug("%d> %s (%s) at 0x%lx", i, vars_table[i].name,
               is_addr(vars_table[i].loctype) ? "GLOBAL" : "STACK",
               is_addr(vars_table[i].loctype) ? vars_table[i].addr : vars_table[i].offset);
}

static inline void
lua_pushohmvalue(lua_State *L, basetype_t *type, void *val)
{
    if (!strcmp(type->name, "double"))
        lua_pushnumber(L, *((double *) val));
    else if (!strcmp(type->name, "int"))
        lua_pushnumber(L, *((int *) val));
    else if (!strcmp(type->name, "char"))
        lua_pushlstring(L, (const char *) val, type->nelem);
    else if (!strcmp(type->name, "long int"))
        lua_pushnumber(L, *((long int *) val));
    else
        derror("error pushing value: invalid basetype");
}

// check if the given "ip" spans within the given function f
static inline int
in_function(function_t *f, unsigned long ip)
{
    //ddebug("fname: %s, f->lowpc = %p, f->hipc = %p, f->ip = %p",
    //       f->name, (void*)f->lowpc, (void*)f->hipc, (void*)ip);
    return (f && ((ip >= f->lowpc) && (ip < f->hipc)));
}

// check if the given "ip" is in the "main()" function
static inline int
in_main(unsigned long ip)
{
    // cache the main fn
    if (!main_fn)
        main_fn = get_function("main");

    return in_function(main_fn, ip);
}

// allocate a new probe
static probe_t *
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
static int
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
static void
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

static int
add_var_location(variable_t *var, Dwarf_Debug dbg, Dwarf_Die die,
                 Dwarf_Attribute attr, Dwarf_Half form)
{
    Dwarf_Locdesc *llbuf = 0, **llbufarray = 0;
    Dwarf_Signed nelem;
    Dwarf_Error err;
    int i, ret = DW_DLV_ERROR, k;
    Dwarf_Half nops = 0;
    Dwarf_Unsigned opd1;
    signed long stack[32];
    int si;

    if (!var)
        return -1;

    si = 0;
    stack[si] = 0;
    stack[++si] = 0;
    var->loctype = 0;
    var->addr = 0;

    ret = dwarf_loclist_n(attr, &llbufarray, &nelem, &err);
    if (ret == DW_DLV_ERROR) {
        derror("error in dwarf_loclist_n().");
        return -1;
    } else if (ret == DW_DLV_NO_ENTRY)
        return 0;

    for (i = 0; i < nelem; ++i) {
        llbuf = llbufarray[i];
        nops = llbuf->ld_cents;
        for (k = 0; k < nops; k++) {
            Dwarf_Loc *expr = &llbuf->ld_s[k];
            Dwarf_Small op = expr->lr_atom;
            opd1 = expr->lr_number;

            switch (op) {
                case DW_OP_addr:
                    var->loctype = OHM_ADDRESS;
                    stack[++si] = opd1;
                    break;
                case DW_OP_fbreg:
                    var->loctype = OHM_FBREG;
                    stack[++si] = opd1;
                    break;
                case DW_OP_reg0:
                case DW_OP_reg1:
                case DW_OP_reg2:
                case DW_OP_reg3:
                case DW_OP_reg4:
                case DW_OP_reg5:
                case DW_OP_reg6:
                case DW_OP_reg7:
                case DW_OP_reg8:
                case DW_OP_reg9:
                case DW_OP_reg10:
                case DW_OP_reg11:
                case DW_OP_reg12:
                case DW_OP_reg13:
                case DW_OP_reg14:
                case DW_OP_reg15:
                case DW_OP_reg16:
                case DW_OP_reg17:
                case DW_OP_reg18:
                case DW_OP_reg19:
                case DW_OP_reg20:
                case DW_OP_reg21:
                case DW_OP_reg22:
                case DW_OP_reg23:
                case DW_OP_reg24:
                case DW_OP_reg25:
                case DW_OP_reg26:
                case DW_OP_reg27:
                case DW_OP_reg28:
                case DW_OP_reg29:
                case DW_OP_reg30:
                case DW_OP_reg31:
                    var->loctype = OHM_REG;
                    stack[++si] = (opd1 - DW_OP_reg0);
                    break;
                case DW_OP_lit0:
                case DW_OP_lit1:
                case DW_OP_lit2:
                case DW_OP_lit3:
                case DW_OP_lit4:
                case DW_OP_lit5:
                case DW_OP_lit6:
                case DW_OP_lit7:
                case DW_OP_lit8:
                case DW_OP_lit9:
                case DW_OP_lit10:
                case DW_OP_lit11:
                case DW_OP_lit12:
                case DW_OP_lit13:
                case DW_OP_lit14:
                case DW_OP_lit15:
                case DW_OP_lit16:
                case DW_OP_lit17:
                case DW_OP_lit18:
                case DW_OP_lit19:
                case DW_OP_lit20:
                case DW_OP_lit21:
                case DW_OP_lit22:
                case DW_OP_lit23:
                case DW_OP_lit24:
                case DW_OP_lit25:
                case DW_OP_lit26:
                case DW_OP_lit27:
                case DW_OP_lit28:
                case DW_OP_lit29:
                case DW_OP_lit30:
                case DW_OP_lit31:
                    var->loctype = OHM_LITERAL;
                    stack[++si] = (opd1 - DW_OP_lit0);
                    break;
                case DW_OP_stack_value:
                    // the previos OP must have set the literal correctly,
                    // so we simply continue.
                    continue;
                case DW_OP_deref:
                    break;

                case DW_OP_const1u:
                case DW_OP_const2u:
                case DW_OP_const4u:
                case DW_OP_const8u:
                case DW_OP_constu:
                    stack[++si] = opd1;
                    break;

                case DW_OP_const1s:
                case DW_OP_const2s:
                case DW_OP_const4s:
                case DW_OP_consts:
                    stack[++si] = opd1;
                    break;

                case DW_OP_dup:
                    stack[si+1] = stack[si];
                    si++;
                    break;
                case DW_OP_plus:
                    stack[si-1] += stack[si];
                    si--;
                    break;
                case DW_OP_plus_uconst:
                    stack[si] += opd1;
                    break;
                case DW_OP_minus:
                    stack[si-1] -= stack[si];
                    si--;
                    break;
                case DW_OP_GNU_push_tls_address:
                    stack[si]++;
                    break;
                default:
                    derror("DWARF-4 is not fully supported. Unsupported OP (0x%x).", op);
                    return -1;
            }
        }

        dwarf_dealloc(dbg, llbufarray[i]->ld_s, DW_DLA_LOC_BLOCK);
        dwarf_dealloc(dbg, llbufarray[i], DW_DLA_LOCDESC);
    }

    if (is_addr(var->loctype) || is_reg(var->loctype) || is_literal(var->loctype))
        var->addr = (Dwarf_Addr) stack[si];
    else if (is_fbreg(var->loctype))
        var->offset = (Dwarf_Signed) stack[si];
    else {
        derror("Unknown DWARF location type. OP (0x%x)", llbufarray[0]->ld_s[0].lr_atom);
        return -1;
    }
    dwarf_dealloc(dbg, llbufarray, DW_DLA_LIST);
    return 1;
}


static int
add_var_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die child_die)
{
    int ret = DW_DLV_ERROR;
    char pname[64], cname[64];
    Dwarf_Error err = 0;
    Dwarf_Off offset = 0;
    Dwarf_Half tag = 0, attrcode, form;
    Dwarf_Attribute *attrs;
    Dwarf_Signed attrcount, i;
    Dwarf_Unsigned bsz = 0;
    Dwarf_Addr lowpc = 0, highpc = 0;
    variable_t *var;
    int saw_lopc = 0;
    int saw_hipc = 0;

    ret = dwarf_tag(child_die, &tag, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_tag()");
        goto error;
    }

    if ((tag != DW_TAG_variable) && (tag != DW_TAG_subprogram))
        return -1;

    if (dwarf_attrlist(child_die, &attrs, &attrcount, &err) != DW_DLV_OK) {
        derror("error in dwarf_attrlist()");
        goto error;
    }

    switch (tag) {
        case DW_TAG_variable:
            var = &vars_table[vars_table_size];
            for (i = 0; i < attrcount; ++i) {
                if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatattr()");
                    goto error;
                }

                if (attrcode == DW_AT_location) {
                    if (dwarf_whatform(attrs[i], &form, &err) != DW_DLV_OK) {
                        derror("error in dwarf_whatform()");
                        goto error;
                    }

                    if (is_location_form(form) || form == DW_FORM_exprloc) {
                        // bail out if we cannot figure out the locdesc
                        if ((ret = add_var_location(var, dbg, child_die, attrs[i], form)) < 0)
                            goto error;
                    } else {
                        derror("unknown dwarf form: %d", form);
                        goto error;
                    }
                }

                if (attrcode == DW_AT_type) {
                    ret = dwarf_formref(attrs[i], &offset, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_formref()");
                        goto error;
                    }

                    if (get_child_name(dbg, child_die, cname, 64) >= 0) {
                        if (get_parent_name(dbg, parent_die, pname, 64) >= 0) {
                            sprintf(var->name, "%s.%s", pname, cname);
                            var->function = get_function(pname);
                        } else {
                            sprintf(var->name, "%s", cname);
                            var->function = NULL;
                        }
                    }
                    
                    var->type = get_type(offset);
                }
            }
            // skip variables whose types we cannot figure out.
            if (var->type)
                vars_table_size++;
            break;
        case DW_TAG_subprogram:
            for (i = 0; i < attrcount; ++i) {
                if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatattr()");
                    return -1;
                }

                if (dwarf_whatform(attrs[i], &form, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatform()");
                    return -1;
                }

                if (attrcode == DW_AT_low_pc) {
                    saw_lopc = 1;
                    dwarf_formaddr(attrs[i], &lowpc, &err);
                    if (saw_hipc)
                        highpc += lowpc;
                }

                if (attrcode == DW_AT_high_pc) {
                    // If Dwarf4
                    if (form != DW_FORM_addr && form == DW_FORM_data8) {
                        get_number(attrs[i], &bsz);
                        highpc = bsz;
                        if (saw_lopc)
                            highpc += lowpc;
                    } else
                        dwarf_formaddr(attrs[i], &highpc, &err);

                    saw_hipc = 1;
                }

                if (saw_lopc && saw_hipc) {
                    /* We construct a table of functions here so that
                     * we can index it later to find the stack probes
                     * to activate. */
                    get_child_name(dbg, child_die, fns_table[fns_table_size].name, 128);
                    fns_table[fns_table_size].lowpc = lowpc;
                    fns_table[fns_table_size].hipc = highpc;
                    fns_table_size++;
                }
            }
            break;
        default:
            break;
    }

    return 1;

error:
    derror("error adding variable %s", vars_table[vars_table_size].name);
    return -1;
}


static int
add_complextype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR;
    Dwarf_Error err = 0;
    Dwarf_Off offset = 0;
    Dwarf_Half tag = 0, attrcode;
    Dwarf_Attribute *attrs, *cattrs;
    Dwarf_Signed attrcount, c, i;
    Dwarf_Unsigned bsz = 0, tid = 0;
    Dwarf_Die grandchild;
    basetype_t *type;

    ret = dwarf_tag(die, &tag, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_tag()");
        goto error;
    }

    if ((tag != DW_TAG_array_type) && (tag != DW_TAG_structure_type) &&
        (tag != DW_TAG_typedef) && (tag != DW_TAG_pointer_type))
        return -1;

    if (dwarf_attrlist(die, &attrs, &attrcount, &err) != DW_DLV_OK) {
        derror("error in dwarf_attrlist()");
        goto error;
    }

    switch (tag) {
        case DW_TAG_array_type:
            for (i = 0; i < attrcount; ++i) {
                if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatattr()");
                    goto error;
                }

                if (attrcode == DW_AT_type) {
                    // get the offset
                    ret = dwarf_die_CU_offset(die, &offset, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_die_CU_offset()");
                        goto error;
                    }

                    // get the type
                    ret = dwarf_formref(attrs[i], &tid, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_formref()");
                        goto error;
                    }

                    dwarf_child(die, &grandchild, &err);
                    ret = dwarf_attrlist(grandchild, &cattrs, &c, &err);
                    if (ret == DW_DLV_OK) {
                        while (--c > 0) {
                            if (dwarf_whatattr(cattrs[c], &attrcode, &err) != DW_DLV_OK) {
                                derror("error in dwarf_whatattr()");
                                goto error;
                            }

                            if (attrcode == DW_AT_upper_bound)
                                get_number(cattrs[c], &bsz);
                        }
                    } else
                        bsz = -1;


                    types_table[types_table_size].id = offset;
                    type = get_type(tid);
                    if (type) {
                        strncpy(types_table[types_table_size].name, type->name, 128);
                        types_table[types_table_size].istypedef = type->istypedef;
                    } else {
                        strncpy(types_table[types_table_size].name, "<unknown-array>", 128);
                        types_table[types_table_size].istypedef = false;
                    }
                    types_table[types_table_size].size = basetype_get_size(type);
                    types_table[types_table_size].nelem = bsz+1;
                    types_table_size++;
                }
            }
            break;

        case DW_TAG_structure_type:
            for (i = 0; i < attrcount; ++i) {
                if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatattr()");
                    goto error;
                }

                if (attrcode == DW_AT_byte_size) {
                    ret = dwarf_die_CU_offset(die, &offset, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_die_CU_offset()");
                        goto error;
                    }

                    types_table[types_table_size].id = offset;
                    ret = get_child_name(dbg, die, types_table[types_table_size].name, 128);
                    if (ret < 0)
                        strncpy(types_table[types_table_size].name, "<unknown-struct>", 128);
                    get_number(attrs[i], &bsz);
                    types_table[types_table_size].size = bsz;
                    types_table[types_table_size].nelem = 1;
                    types_table[types_table_size].istypedef = false;
                    types_table_size++;
                }

                // get the tag members now, need a new type structure maybe?
            }
            break;

        case DW_TAG_typedef:
            for (i = 0; i < attrcount; ++i) {
                if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatattr()");
                    goto error;
                }

                if (attrcode == DW_AT_type) {
                    ret = dwarf_die_CU_offset(die, &offset, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_die_CU_offset()");
                        goto error;
                    }

                    // get the type
                    ret = dwarf_formref(attrs[i], &tid, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_formref()");
                        goto error;
                    }

                    types_table[types_table_size].id = offset;
                    ret = get_child_name(dbg, die, types_table[types_table_size].name, 128);
                    if (ret < 0)
                        strncpy(types_table[types_table_size].name, "<unknown-typedef>", 128);
                    types_table[types_table_size].size = 0;
                    // nelem indicates the base type ID here.
                    types_table[types_table_size].nelem = tid;
                    types_table[types_table_size].istypedef = true;
                    types_table_size++;
                }
            }
            break;

        case DW_TAG_pointer_type:
            for (i = 0; i < attrcount; ++i) {
                if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
                    derror("error in dwarf_whatattr()");
                    goto error;
                }

                if (attrcode == DW_AT_type) {
                    ret = dwarf_die_CU_offset(die, &offset, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_die_CU_offset()");
                        goto error;
                    }

                    // get the type
                    ret = dwarf_formref(attrs[i], &tid, &err);
                    if (ret != DW_DLV_OK) {
                        derror("error in dwarf_formref()");
                        goto error;
                    }

                    types_table[types_table_size].id = offset;
                    type = get_type(tid);
                    if (type)
                        snprintf(types_table[types_table_size].name, 128, "%s*", type->name);
                    else
                        strncpy(types_table[types_table_size].name, "<unknown-ptr>", 128);
                    types_table[types_table_size].size = 0;
                    // nelem indicates the base type ID here.
                    types_table[types_table_size].nelem = tid;
                    types_table[types_table_size].istypedef = true;
                    types_table_size++;
                }
            }
            break;
            
        default:
            break;
    }

    return 1;

error:
    derror("error in add_complextype_from_die()");
    return -1;
}

static int
add_basetype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR;
    Dwarf_Error err = 0;
    Dwarf_Off offset = 0;
    Dwarf_Half tag = 0, attrcode;
    Dwarf_Attribute *attrs;
    Dwarf_Signed attrcount, i;
    Dwarf_Unsigned bsz = 0;

    ret = dwarf_tag(die, &tag, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_tag()");
        goto error;
    }

    if (tag != DW_TAG_base_type)
        return -1;

    if (dwarf_attrlist(die, &attrs, &attrcount, &err) != DW_DLV_OK) {
        derror("error in dwarf_attrlist()");
        goto error;
    }

    for (i = 0; i < attrcount; ++i) {
        if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
            derror("error in dwarf_whatattr()");
            goto error;
        }

        if (attrcode == DW_AT_byte_size) {
            ret = dwarf_die_CU_offset(die, &offset, &err);
            if (ret != DW_DLV_OK) {
                derror("error in dwarf_die_CU_offset()");
                goto error;
            }

            /* We construct a table of base types here so that we can
             * index it later to find the types of some of the probes
             * on the stack. */
            types_table[types_table_size].id = offset;
            get_child_name(dbg, die, types_table[types_table_size].name, 128);
            get_number(attrs[i], &bsz);
            types_table[types_table_size].size = bsz;
            types_table[types_table_size].nelem = 1;
            types_table[types_table_size].istypedef = false;
            types_table_size++;
        }
    }

    return 1;

error:
    derror("error in add_basetype_from_die()");
    return -1;
}

static int
traverse_die(dwarf_query_cb_t cb, Dwarf_Debug dbg, Dwarf_Die parent_die,
             Dwarf_Die child_die)
{
    int ret = DW_DLV_ERROR;
    Dwarf_Die cur_die = child_die, child;
    Dwarf_Error err;

    (*cb)(dbg, parent_die, child_die);

    while (1) {
        Dwarf_Die sib_die = 0;
        ret = dwarf_child(cur_die, &child, &err);
        if (ret == DW_DLV_ERROR) {
            derror("error in dwarf_child()");
            return -1;
        } else if (ret == DW_DLV_OK)
            traverse_die(cb, dbg, cur_die, child);

        ret = dwarf_siblingof(dbg, cur_die, &sib_die, &err);
        if (ret == DW_DLV_ERROR) {
            derror("error in dwarf_siblingof()");
            return -1;
        } else if (ret == DW_DLV_NO_ENTRY)
            break;

        if (cur_die != child_die)
            dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);

        cur_die = sib_die;
        (*cb)(dbg, parent_die, cur_die);
    }
    return 1;
}

// scan for all types or variables and  function in a given file
// "file". The debug information defined by the DWARF format is used
// to fetch all of the symbols from within the file. We make a list of
// the types or functions and variables in the file.
static int
scan_file(char *file, dwarf_query_cb_t cb)
{
    int ret = DW_DLV_ERROR, fd = -1;
    Dwarf_Debug dbg = 0;
    Dwarf_Error err;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;
    Dwarf_Unsigned cu_hdr_len, abbr_off, next_cu_hdr;
    Dwarf_Half ver_stamp, addr_sz;
    Dwarf_Die cu_die;

    fd = open(file, O_RDONLY);
    if (fd < 0) {
        derror("error reading file %s.", file);
        goto error;
    }

    if (dwarf_init(fd, DW_DLC_READ, errhand, errarg, &dbg, &err) != DW_DLV_OK) {
        derror("dwarf_init() failed.");
        goto error;
    }

    while (1) {
        ret = dwarf_next_cu_header(dbg, &cu_hdr_len, &ver_stamp, &abbr_off,
                                   &addr_sz, &next_cu_hdr, &err);
        if (ret == DW_DLV_ERROR) {
            derror("error reading DWARF CU header.");
            goto error;
        } else if (ret == DW_DLV_NO_ENTRY)
            break;

        if (dwarf_siblingof(dbg, NULL, &cu_die, &err) == DW_DLV_ERROR)
            derror("error getting sibling of cu.");

        ret = traverse_die(cb, dbg, NULL, cu_die);
        dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
    }

    if (dwarf_finish(dbg, &err) != DW_DLV_OK) {
        derror("dwarf_finish() failed.");
        goto error;
    }

    close(fd);
    return 1;

error:
    close(fd);
    derror("invalid dwarf file %s.", file);
    return -1;
}

// read the corresponding ohm recipe file and load the Lua language
// runtime.
static int
ohmread(char *path, probe_t **probes)
{
    int         np;
    char       *probe;
    variable_t *v;
    probe_t    *p;

    np = 0;
    L = luaL_newstate();

    luaL_openlibs(L);
    if (luaL_loadfile(L, "ohm.lua") || lua_pcall(L, 0, 0, 0)) {
        derror("%s", lua_tostring(L, -1));
        goto error;
    }

    if (luaL_loadfile(L, path) || lua_pcall(L, 0, 0, 0)) {
        derror("%s", lua_tostring(L, -1));
        goto error;
    }

    lua_getglobal(L, "probes");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        goto error;
    }

    // table is on the stack at index -1
    lua_pushnil(L);  // first key
    while (lua_next(L, -2) != 0) {
        // name is at index -2 and probe struct at index -1
        probe = (char *) lua_tostring(L, -2);
        lua_pop(L, 1);
        v = get_variable(probe);
        if (!v) {
            ddebug("Skipping non-existent probe %s.", probe);
            continue;
        }

        p = new_probe(v, 1);
        if (p && probes_list_add(&probes_list, p) < 0)
            continue;
        else
            np++;
    }
    lua_pop(L, 1);
    return np;
error:
    derror("invalid ohm file %s.", path);
    return -1;
}

#if HAVE_XPMEM
static int
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

static int
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

static void
xpmem_detach_mem(void)
{
    int i;
    for (i = 0; i < xpmem_nseg; i++) {
        xpmem_detach(xpmem_segs[i].addr);
        xpmem_release(xpmem_segs[i].ap);
        xpmem_remove(xpmem_segs[i].seg);
    }
}
#endif

static void
usage(void)
{
    fprintf(stderr, "usage: " PACKAGE_NAME " [-D] [-o ohmfile]\n"
                    "\t [-i interval] <program> <args>\n\n");
    fprintf(stderr, "Report bugs to " PACKAGE_BUGREPORT);
    exit(1);
}

static int
remote_read(probe_t *probe, addr_t addr, void *arg)
{
    int ret;
    size_t size = basetype_get_size(probe->var->type) * basetype_get_nelem(probe->var->type);
    ddebug("mem read from 0x%lx to %p, size (%lu)", addr, probe->buf, size);

#if HAVE_CMA
    struct iovec local[1], remote[1];
    pid_t pid;

    local[0].iov_base = probe->buf;
    local[0].iov_len = size;
    remote[0].iov_base = (void*)addr;
    remote[0].iov_len = size;
    USED(arg);
    ret = 0;
    do {
        ret += process_vm_readv(ohm_cpid, local, 1, remote, 1, 0);
    } while (ret > 0 && ret < size);
#elif HAVE_XPMEM
    USED(arg);
    ret = xpmem_copy(probe->buf, (void*)addr, size);
#else
    int i;
    for (i = 0; (i<<3) < size; i++)
        ret = _UPT_access_mem(unw_addrspace, addr+(i<<3),
                              (unw_word_t*)(probe->buf+(i<<3)), 0, arg);
#endif
    return ret;
}

static int
write_lua(probe_t *probe, addr_t addr, void *arg)
{
    int ret, i;
    size_t size;

    if (!probe)
        return 0;

    size = basetype_get_size(probe->var->type) * basetype_get_nelem(probe->var->type);
    
    lua_pushstring(L, probe->var->name);
    if (probe->var->type->nelem > 1)
        lua_newtable(L);

    ret = remote_read(probe, addr, arg);
    if (ret < 0)
        return ret;

    for (i = 0; (i<<3) < size; i++) {
        if (probe->var->type->nelem > 1)
            lua_pushnumber(L, i+1);

        lua_pushohmvalue(L, probe->var->type, probe->buf+(i<<3));

        if (probe->var->type->nelem > 1)
            lua_rawset(L, -3);
    }

    lua_rawset(L, -3);
    return ret;
}

static void
probe(void *arg)
{
    probe_t *p;
    unw_word_t ip, ptr;
    unw_cursor_t cur;

    lua_getglobal(L, "ohm_add");
    if(!lua_isfunction(L, -1)) {
        lua_pop(L,1);
        return;
    }

    lua_newtable(L);
    for (p = probes_list; p != NULL; p = p->next) {
        switch (p->var->loctype) {
            case OHM_ADDRESS:
                if (write_lua(p, p->var->addr, arg) < 0) 
                    derror("error in probe, skipping...");
                break;

            default:
                // copy the cursor so we can move back
                cur = unw_cursor;
                do {
                    // unwind the stack to read as many
                    // probes as possible
                    unw_get_reg(&cur, UNW_REG_IP, &ip);
                    if (in_function(p->var->function, ip)) {
                        // Get the probe location
                        if (is_fbreg(p->var->loctype)) {
                            unw_get_reg(&cur, UNW_X86_64_RBP, &ptr);
                            ptr = ptr+16+p->var->offset;
                        } else if (is_reg(p->var->loctype)) {
                            unw_get_reg(&cur, p->var->offset, &ptr);
                        } else if (is_literal(p->var->loctype)) {
                            ptr = p->var->offset;
                        } else
                            derror("don't know how to read probe, skipping...");
                        // copy it to the Lua Land.
                        if (write_lua(p, ptr, arg) < 0)
                            derror("error in accessing probe, skipping...");
                    }
                } while (!in_main(ip) && (unw_step(&cur) > 0));
                break;
        }
    }

    if (lua_pcall(L, 1, 0, 0) != 0) {
        derror("error adding value: %s\n", lua_tostring(L, -1));
        return;
    }
}

void ohm_cleanup(int sig)
{
    ohm_shutdown = true;
    // ask the probed process to terminate
    if (kill(ohm_cpid, SIGTERM) < 0)
        perror("kill");
    waitpid(ohm_cpid, 0, WNOHANG);
#if HAVE_XPMEM
    xpmem_detach_mem();
#endif
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    char *s, *ohmfile;
    int c, ret, status;
    struct timespec ts;
    void *upt_info;

    ohmfile = DEFAULT_OHMFILE;
    while ((c = getopt(argc, argv, "Do:i:h")) != -1) {
        switch (c) {
            case 'D':
                doctor_debug = 1;
                break;
            case 'o':
                ohmfile = optarg;
                break;
            case 'i':
                doctor_interval = strtod(optarg, &s);
                if (*s != '\0')
                    usage();
                break;
            case 'h':
            default:
                usage();
        }
    }

    if ((argc - optind) < 1)
        usage();

    if (!doctor_interval) {
        derror("invalid interval %f.", doctor_interval);
        goto error;
    }
    ddebug("setting doctor interval to %.3f seconds.", doctor_interval);

    // First we scan for the functions and types.
    if ((ret = scan_file(argv[optind], &add_basetype_from_die)) < 0) {
        derror("error scanning types from %s. (compile with -g)",
               argv[optind]);
        goto error;
    }
    ddebug("%d base types found.", types_table_size);

    if ((ret = scan_file(argv[optind], &add_complextype_from_die)) < 0) {
        derror("error scanning types from %s. (compile with -g)",
               argv[optind]);
        goto error;
    }
    ddebug("%d complex types found.", types_table_size);

    // print the types table
    // for (c = 0; c < types_table_size; c++)
    //     printf("%d :: %s[%d] %lu (TID %d)\n", c, types_table[c].name,
    //            types_table[c].nelem, types_table[c].size,
    //            types_table[c].id);

    //  next we look for the variables.
    if ((ret = scan_file(argv[optind], &add_var_from_die)) < 0) {
        derror("error scanning variables from %s. (compile with -g)",
               argv[optind]);
        goto error;
    }
    ddebug("%d variables found.", vars_table_size);
    // print_all_variables();

    // And finally, we read the OHM prescription.
    ddebug("reading ohm prescription: %s.", ohmfile);
    if ((ret = ohmread(ohmfile, &probes_list)) < 0) {
        derror("error reading ohm prescription %s.", ohmfile);
        goto error;
    } else
        ddebug("%d probes requested.", ret);

    print_probes(probes_list);
    ohm_shutdown = false;
    signal(SIGINT, ohm_cleanup);
    signal(SIGTERM, ohm_cleanup);

    switch(ohm_cpid = fork()) {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
        case 0:
            /* child */
            ptrace(PTRACE_TRACEME, 0, 0, 0);
            execvp(argv[optind], &argv[optind]);
            derror("Can't execute `%s': %s", argv[1], strerror(errno));
            exit(2);
        default:
            /* parent */
            while (wait(&status) < 0) {
                if (errno == ECHILD)
                    exit(0);
                else if (errno == EINTR)
                    continue;
                else {
                    perror("wait");
                    exit(1);
                }
            }

#if HAVE_XPMEM
            if (xpmem_attach_mem(ohm_cpid) < 0) {
                derror("error mapping remote process's memory.");
                goto error;
            }
#endif
            ddebug("Probing process %u.", ohm_cpid);
            // create the unwind address space
            unw_addrspace = unw_create_addr_space(&_UPT_accessors, 0);
            if (!unw_addrspace) {
                derror("unable to create unwind address space.");
                goto error;
            }

            // create UPT-info structure
            upt_info = _UPT_create(ohm_cpid);
            if (!upt_info) {
                derror("error creating _UPT-info structure.");
                goto error;
            }

            ts.tv_sec = (int) doctor_interval;
            ts.tv_nsec = (doctor_interval - ts.tv_sec) * 1E9;

            while (!WIFEXITED(status) && !WIFSIGNALED(status) && !ohm_shutdown) {
                ret = unw_init_remote(&unw_cursor, unw_addrspace, upt_info);
                if (ret < 0) {
                    derror("error initializing remote upt ptrace.");
                    goto error;
                }

                if (WIFSTOPPED(status))
                    _UPT_resume(unw_addrspace, &unw_cursor, upt_info);

                nanosleep(&ts, NULL);
                if (kill(ohm_cpid, SIGSTOP) < 0)
                    perror("kill");
                waitpid(-1, &status, 0);

                if (WIFEXITED(status))
                    break;

                probe(upt_info);
            }

            _UPT_destroy(upt_info);
    }

#if HAVE_XPMEM
    xpmem_detach_mem();
#endif
    return 0;

error:
    derror("Exiting doctor.");
    return -1;
}
