// Copyright (c) 2010-2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "ohmd.h"

int
is_location_form(int form)
{
    if (form == DW_FORM_block1 || form == DW_FORM_block2 ||
        form == DW_FORM_block4 || form == DW_FORM_block ||
        form == DW_FORM_data4 || form == DW_FORM_data8 ||
        form == DW_FORM_sec_offset) {
        return 1;
    }
    return 0;
}

void
get_number(Dwarf_Attribute attr, Dwarf_Unsigned *val)
{
    Dwarf_Error err = 0;
    int ret;

    Dwarf_Unsigned uval = 0;
    ret = dwarf_formudata(attr, &uval, &err);
    if (ret == DW_DLV_OK) {
        *val = uval;
        return;
    }

    Dwarf_Signed sval = 0;
    ret = dwarf_formsdata(attr, &sval, &err);
    if (ret == DW_DLV_OK) {
        *val = sval;
        return;
    }
}

int
get_child_name(Dwarf_Debug dbg, Dwarf_Die child, char *name, int size)
{
    Dwarf_Error err;
    char *cname;

    if (!name || !child)
        return -1;

    if (dwarf_diename(child, &cname, &err) != DW_DLV_OK)
        return -1;

    strncpy(name, cname, size);
    dwarf_dealloc(dbg, cname, DW_DLA_STRING);
    return 0;
}

int
get_parent_name(Dwarf_Debug dbg, Dwarf_Die parent, char *name, int size)
{
    int ret;
    Dwarf_Error err;

    if (!name || !parent)
        return -1;

    Dwarf_Half tag;
    ret = dwarf_tag(parent, &tag, &err);
    if (ret != DW_DLV_OK || tag == DW_TAG_compile_unit)
        return -1;

    char *pname;
    ret = dwarf_diename(parent, &pname, &err);
    if (ret != DW_DLV_OK)
        return -1;

    strncpy(name, pname, size);
    dwarf_dealloc(dbg, pname, DW_DLA_STRING);
    return 0;
}

int
get_offset_tid(Dwarf_Die die, Dwarf_Off *offset, Dwarf_Unsigned *tid)
{
    int ret;
    Dwarf_Error err;
    Dwarf_Attribute attr;

    // get the offset
    ret = dwarf_die_CU_offset(die, offset, &err);
    if (ret == DW_DLV_ERROR)
        return -1;

    // get the type
    ret = dwarf_attr(die, DW_AT_type, &attr, &err);
    if (ret == DW_DLV_ERROR)
        return -1;

    if (ret == DW_DLV_OK)
        if ((dwarf_formref(attr, tid, &err)) != DW_DLV_OK)
            return -1;

    return 0;
}
