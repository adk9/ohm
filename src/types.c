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
    if (type->istypedef)
        return basetype_get_size(get_type(type->nelem));
    else
        return type->size;
}

inline unsigned int
basetype_get_nelem(basetype_t *type)
{
    if (!type)
        return 0;
    if (type->istypedef)
        return basetype_get_nelem(get_type(type->nelem));
    else
        return type->nelem;
}

int
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

int
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
