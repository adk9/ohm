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
#include <dwarf.h>

#include "ohmd.h"

// Variable table
variable_t   vars_table[OHM_MAX_NUM_VARS];
unsigned int vars_table_size;

// Function table
function_t   fns_table[OHM_MAX_NUM_FUNCTIONS];
unsigned int fns_table_size;

// The "main" function
function_t  *main_fn;

// get a variable object for a variable given its name
variable_t*
get_variable(char *name)
{
    int i;
    for (i = 0; i < vars_table_size; i++) {
        if (!strcmp(name, vars_table[i].name))
            return &vars_table[i];
    }
    return NULL;
}

// get the function object for a function given its name
function_t*
get_function(char *name)
{
    int i;
    for (i = 0; i < fns_table_size; i++) {
        if (!strcmp(name, fns_table[i].name))
            return &fns_table[i];
    }
    return NULL;
}

void
print_all_variables(void)
{
    int i;
    for (i = 0; i < vars_table_size; i++)
        ddebug("%d> %s (%s) at 0x%lx (tid: %d)", i, vars_table[i].name,
               is_addr(vars_table[i].loctype) ? "GLOBAL" : "STACK",
               is_addr(vars_table[i].loctype) ? vars_table[i].addr : vars_table[i].offset,
               vars_table[i].type->id);
}

void
print_all_functions(void)
{
    int i;
    for (i = 0; i < fns_table_size; i++)
        ddebug("%d> %s 0x%lx-0x%lx", i, fns_table[i].name,
               fns_table[i].lowpc, fns_table[i].hipc);
}

// check if the given "ip" spans within the given function f
inline int
in_function(function_t *f, unsigned long ip)
{
    return (f && ((ip >= f->lowpc) && (ip < f->hipc)));
}

// check if the given "ip" is in the "main()" function
inline int
in_main(unsigned long ip)
{
    if (!main_fn)
        main_fn = get_function("main");
    return in_function(main_fn, ip);
}

int
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

int
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
    int saw_lopc;
    int saw_hipc;
    size_t size;

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
            if (var->type) {
                vars_table_size++;
                // now we know the type. if it is a struct, hoist the
                // members up as variables
                basetype_t *type = get_type_alias(var->type);
                if (is_struct(type->ohm_type)) {
                    size = 0;
                    for (i = 0; i < type->nelem; ++i) {
                        variable_t *newvar = &vars_table[vars_table_size];
                        sprintf(newvar->name, "%s.%s", var->name, type->elems[i]->name);
                        newvar->type = type->elems[i];
                        newvar->function = var->function;
                        newvar->loctype = var->loctype;
                        if (is_addr(var->loctype)) {
                            newvar->addr = var->addr + size;
                        } else {
                            newvar->offset = var->offset + size;
                        }
                        size = type->elems[i]->size;
                        vars_table_size++;
                    }
                }

            }
            break;
        case DW_TAG_subprogram:
            saw_lopc = 0;
            saw_hipc = 0;
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
                    saw_lopc = 0;
                    saw_hipc = 0;
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
