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

// Basetype table
basetype_t   types_table[OHM_MAX_NUM_TYPES];
unsigned int types_table_size;

// fetch a basetype for an object given its id
basetype_t*
get_type(int id)
{
    int i;
    for (i = 0; i < types_table_size; i++) {
        if (id == types_table[i].id)
            return &types_table[i];
    }
    return NULL;
}

basetype_t*
get_or_add_type(int id)
{
    basetype_t *t;
    t = get_type(id);
    if (!t) {
        t = &types_table[types_table_size++];
        t->id = id;
        t->size = 0;
    }
    return t;
}

inline size_t
get_type_size(basetype_t *type)
{
    if (!type)
        return 0;

    if (is_alias(type->ohm_type) && type->elems)
        return get_type_size(type->elems[0]);

    return type->size;
}

inline unsigned int
get_type_nelem(basetype_t *type)
{
    if (!type)
        return 0;

    if (is_alias(type->ohm_type) && type->elems)
        return get_type_nelem(type->elems[0]);

    return type->nelem;
}

inline basetype_t*
get_type_alias(basetype_t *type)
{
    if (is_alias(type->ohm_type) && type->elems)
        return get_type_alias(type->elems[0]);
    return type;
}

inline int
get_type_ohmtype(basetype_t *type)
{
    if (!type)
        return -1;

    if (!strcmp(type->name, "int"))
        return OHM_TYPE_INT;
    else if (!strcmp(type->name, "unsigned int"))
        return OHM_TYPE_UINT;
    else if (!strcmp(type->name, "double"))
        return OHM_TYPE_DOUBLE;
    else if (!strcmp(type->name, "float"))
        return OHM_TYPE_FLOAT;
    else if (!strcmp(type->name, "char"))
        return OHM_TYPE_CHAR;
    else if (!strcmp(type->name, "unsigned char"))
        return OHM_TYPE_UCHAR;
    else if (!strcmp(type->name, "long"))
        return OHM_TYPE_LONG;
    else if (!strcmp(type->name, "unsigned long"))
        return OHM_TYPE_ULONG;
    else if (!strcmp(type->name, "ptr"))
        return OHM_TYPE_PTR;
    else if (!strcmp(type->name, "long int"))
        return OHM_TYPE_LONG_INT;
    else if (!strcmp(type->name, "long unsigned int"))
        return OHM_TYPE_LONG_UINT;
    else
        return OHM_TYPE_DOUBLE;
}

static int
add_structmember_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR;
    Dwarf_Error err = 0;
    Dwarf_Half tag = 0;
    Dwarf_Off offset = 0;
    Dwarf_Unsigned tid = 0;
    Dwarf_Unsigned loc = 0;
    basetype_t *t, *t2;

    ret = dwarf_tag(die, &tag, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_tag()");
        goto error;
    }

    if (tag != DW_TAG_member)
        return -1;

    ret = get_offset_tid(die, &offset, &tid);
    if (ret < 0) {
        derror("error in get_offset_tid()");
        goto error;
    }

    t = get_or_add_type(offset);
    ret = get_child_name(dbg, die, t->name, 128);
    if (ret < 0)
        strncpy(t->name, "<unknown-structmbr>", 128);
    t->ohm_type = OHM_TYPE_ALIAS;
    ret = get_member_location(die, &loc);
    if (ret < 0) {
        derror("error in get_member_location()");
        goto error;
    }
    // this is the struct member location and not the size; we will
    // fix it later in refresh_compound_sizes.
    t->size = loc;

    t2 = get_or_add_type(tid);
    t->nelem = 1;
    t->elems = malloc(sizeof(t));
    t->elems[0] = t2;

    return 1;

error:
    derror("error in add_structmember_from_die()");
    return -1;
}


int
add_basetype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR;
    Dwarf_Error err = 0;
    Dwarf_Off offset = 0;
    Dwarf_Unsigned bsz = 0;
    basetype_t *t;

    if (is_base_type(die) != 1)
        return -1;

    ret = dwarf_die_CU_offset(die, &offset, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_die_CU_offset()");
        goto error;
    }

    ret = dwarf_bytesize(die, &bsz, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_bytesize()");
        goto error;
    }

    /* We construct a table of base types here so that we can iœndex it
     * later to find the types of some of the probes on the stack. */

    t = get_or_add_type(offset);
    get_child_name(dbg, die, t->name, 128);
    t->ohm_type = get_type_ohmtype(t);
    t->size = bsz;
    t->nelem = 1;
    t->elems = NULL;
    return 1;

error:
    derror("error in add_basetype_from_die()");
    return -1;
}

int
add_complextype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR, i, nsib;
    Dwarf_Error err = 0;
    Dwarf_Off offset = 0;
    Dwarf_Half tag = 0;
    Dwarf_Attribute attr;
    Dwarf_Unsigned bsz = 0, tid = 0;
    Dwarf_Die grandchild;
    basetype_t *t, *t2;

    ret = dwarf_tag(die, &tag, &err);
    if (ret != DW_DLV_OK) {
        derror("error in dwarf_tag()");
        goto error;
    }

    if ((tag != DW_TAG_array_type) && (tag != DW_TAG_structure_type) &&
        (tag != DW_TAG_typedef) && (tag != DW_TAG_pointer_type))
        return -1;

    switch (tag) {
        case DW_TAG_array_type:
            ret = get_offset_tid(die, &offset, &tid);
            if (ret < 0) {
                derror("error in get_offset_tid()");
                goto error;
            }

            // get the child
            dwarf_child(die, &grandchild, &err);
            ret = dwarf_attr(grandchild, DW_AT_upper_bound, &attr, &err);
            if (ret == DW_DLV_ERROR) {
                derror("error in dwarf_attr(DW_AT_upper_bound)");
                goto error;
            } else if (ret == DW_DLV_OK)
                get_number(attr, &bsz);
            else
                return 0;

            t = get_or_add_type(offset);
            snprintf(t->name, 128, "arr%u[]", (unsigned int)offset);
            t->ohm_type = OHM_TYPE_ARRAY;
            t->nelem = bsz+1;
            t2 = get_or_add_type(tid);
            t->size = t->nelem * get_type_size(t2);
            t->elems = malloc(sizeof(t));
            t->elems[0] = t2;
            break;

        case DW_TAG_structure_type:
            ret = dwarf_die_CU_offset(die, &offset, &err);
            if (ret != DW_DLV_OK) {
                derror("error in dwarf_die_CU_offset()");
                goto error;
            }

            t = get_or_add_type(offset);
            strncpy(t->name, "struct ", 7);
            ret = get_child_name(dbg, die, t->name+7, 128);
            if (ret < 0)
                strncpy(t->name, "<unknown-struct>", 128);
            t->ohm_type = OHM_TYPE_STRUCT;
            ret = dwarf_bytesize(die, &bsz, &err);
            t->size = ((ret == DW_DLV_OK) ? bsz : 0);

            // ensure that struct members get added to the table
            nsib = traverse_die(&add_structmember_from_die, dbg, parent_die, die);
            if (nsib < 0)
                goto error;
            t->nelem = nsib-1;
            t->elems = malloc((t->nelem)*sizeof(t));
            for (i = 0; i < t->nelem; i++)
                t->elems[i] = &types_table[types_table_size-t->nelem-1+i];
            break;

        case DW_TAG_typedef:
            ret = get_offset_tid(die, &offset, &tid);
            if (ret < 0) {
                derror("error in get_offset_tid()");
                goto error;
            }

            t = get_or_add_type(offset);
            t->ohm_type = OHM_TYPE_ALIAS;
            t->size = 0;
            t->nelem = 1;
            t2 = get_or_add_type(tid);
            t->elems = malloc(sizeof(t));
            t->elems[0] = t2;
            ret = get_child_name(dbg, die, t->name, 128);
            if (ret < 0)
                strncpy(t->name, "<unknown-typedef>", 128);
            break;

        case DW_TAG_pointer_type:
            ret = get_offset_tid(die, &offset, &tid);
            if (ret < 0) {
                derror("error in get_offset_tid()");
                goto error;
            }

            t = get_or_add_type(offset);
            strncpy(t->name, "ptr", 128);
            t->ohm_type = OHM_TYPE_PTR;
            t->nelem = 1;
            t->size = sizeof(void*);
            t2 = get_type(tid);
            t->size = get_type_size(t2);
            t->elems = malloc(sizeof(t));
            t->elems[0] = t2;
            break;
            
        default:
            break;
    }

    return 1;

error:
    derror("error in add_complextype_from_die()");
    return -1;
}

void
refresh_compound_sizes(void) {
    int c, sz, i, nmemb;
    basetype_t *t0, *t1;
    for (c = 0; c < types_table_size; c++) {
        if (is_array(types_table[c].ohm_type)) {
            sz = get_type_size(types_table[c].elems[0]);
            types_table[c].size = types_table[c].nelem * sz;
        } else if (is_struct(types_table[c].ohm_type)) {
            nmemb = types_table[c].nelem;
            for (i = 1; i < nmemb-1; ++i) {
                t0 = types_table[c].elems[i];
                t1 = types_table[c].elems[i+1];
                t0->size = t1->size - t0->size;

            }
            t0 = types_table[c].elems[nmemb-1];
            t0->size = types_table[c].size - t0->size;
        }
    }
}
