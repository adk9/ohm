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
basetype_t   types_table[DEFAULT_NUM_TYPES];
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

inline size_t
basetype_get_size(basetype_t *type)
{
    if (!type)
        return 0;
    if (type->elems && type->elems[0])
        return basetype_get_size(type->elems[0]);
    return type->size;
}

inline unsigned int
basetype_get_nelem(basetype_t *type)
{
    if (!type)
        return 0;
    if (type->elems && type->elems[0])
        return basetype_get_nelem(type->elems[0]);
    else
        return type->nelem;
}

static int
add_structmember_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR;
    Dwarf_Error err = 0;
    Dwarf_Half tag = 0;
    Dwarf_Off offset = 0;
    Dwarf_Unsigned tid = 0;

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

    types_table[types_table_size].id = offset;
    ret = get_child_name(dbg, die, types_table[types_table_size].name, 128);
    if (ret < 0)
        strncpy(types_table[types_table_size].name, "<unknown-structmbr>", 128);

    types_table[types_table_size].size = 0;
    types_table[types_table_size].nelem = 1;
    types_table[types_table_size].elems = malloc(sizeof(basetype_t*));
    types_table[types_table_size].elems[0] = get_type(tid);
    types_table_size++;
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

    /* We construct a table of base types here so that we can index it
     * later to find the types of some of the probes on the stack. */
    types_table[types_table_size].id = offset;
    get_child_name(dbg, die, types_table[types_table_size].name, 128);
    types_table[types_table_size].size = bsz;
    types_table[types_table_size].nelem = 1;
    types_table[types_table_size].elems = NULL;
    types_table_size++;

    return 1;

error:
    derror("error in add_basetype_from_die()");
    return -1;
}

int
add_complextype_from_die(Dwarf_Debug dbg, Dwarf_Die parent_die, Dwarf_Die die)
{
    int ret = DW_DLV_ERROR, i;
    Dwarf_Error err = 0;
    Dwarf_Off offset = 0;
    Dwarf_Half tag = 0;
    Dwarf_Attribute attr;
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

            types_table[types_table_size].id = offset;
            type = get_type(tid);
            if (type) {
                strncpy(types_table[types_table_size].name, type->name, 128);
                types_table[types_table_size].elems = type->elems;
            } else {
                strncpy(types_table[types_table_size].name, "<unknown-array>", 128);
                types_table[types_table_size].elems = NULL;
            }
            types_table[types_table_size].size = basetype_get_size(type);
            types_table[types_table_size].nelem = bsz+1;
            types_table_size++;
            break;

        case DW_TAG_structure_type:
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

            // ensure that the struct members get added to the table
            ret = traverse_die(&add_structmember_from_die, dbg, NULL, die);
            if (ret < 0)
                goto error;

            types_table[types_table_size].id = offset;
            types_table[types_table_size].nelem = ret;
            types_table[types_table_size].size = bsz;
            types_table[types_table_size].elems = malloc(ret * sizeof(basetype_t*));
            for (i = 0; i < ret; i++)
                types_table[types_table_size].elems[i] = &types_table[types_table_size-ret+i];

            ret = get_child_name(dbg, die, types_table[types_table_size].name, 128);
            if (ret < 0)
                strncpy(types_table[types_table_size].name, "<unknown-struct>", 128);
            types_table_size++;
            break;

        case DW_TAG_typedef:
            ret = get_offset_tid(die, &offset, &tid);
            if (ret < 0) {
                derror("error in get_offset_tid()");
                goto error;
            }

            types_table[types_table_size].id = offset;
            ret = get_child_name(dbg, die, types_table[types_table_size].name, 128);
            if (ret < 0)
                strncpy(types_table[types_table_size].name, "<unknown-typedef>", 128);
            types_table[types_table_size].size = 0;
            types_table[types_table_size].nelem = 1;
            types_table[types_table_size].elems = malloc(sizeof(basetype_t*));
            types_table[types_table_size].elems[0] = get_type(tid);
            types_table_size++;
            break;

        case DW_TAG_pointer_type:
            ret = get_offset_tid(die, &offset, &tid);
            if (ret < 0) {
                derror("error in get_offset_tid()");
                goto error;
            }

            types_table[types_table_size].id = offset;
            type = get_type(tid);
            if (type) {
                snprintf(types_table[types_table_size].name, 128, "%s*", type->name);
                types_table[types_table_size].elems = malloc(sizeof(basetype_t*));
                types_table[types_table_size].elems[0] = type;
            } else {
                strncpy(types_table[types_table_size].name, "<unknown-ptr>", 128);
                types_table[types_table_size].elems = NULL;
            }
            types_table[types_table_size].size = 0;
            types_table[types_table_size].nelem = 1;
            types_table_size++;
            break;
            
        default:
            break;
    }

    return 1;

error:
    derror("error in add_complextype_from_die()");
    return -1;
}
