#ifndef _DWARF_UTIL_H
#define _DWARF_UTIL_H

#include <dwarf.h>
#include <libdwarf.h>

/******************************************/
/* DWARF utility functions for the doctor */
/******************************************/

// determine whether the given DWARF form is a location
int is_location_form(int form);

// get the number denoted by a DWARF attribute 'attr'
void get_number(Dwarf_Attribute attr, Dwarf_Unsigned *val);

// get name of the child die
int get_child_name(Dwarf_Debug dbg, Dwarf_Die child, char *name, int size);

// get name of a parent die (this also ensures the parent die is not a
// compile unit
int get_parent_name(Dwarf_Debug dbg, Dwarf_Die parent, char *name, int size);

#endif /* _DWARF_UTIL_H */
