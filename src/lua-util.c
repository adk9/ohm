// Copyright (c) 2010-2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "ohmd.h"

inline void
lua_pushbuf(lua_State *L, basetype_t *type, void *val)
{
    switch (type->ohm_type) {
        case OHM_TYPE_INT:
            lua_pushnumber(L, *((int *) val));
            break;
        case OHM_TYPE_UINT:
            lua_pushnumber(L, *((unsigned int *) val));
            break;
        case OHM_TYPE_DOUBLE:
            lua_pushnumber(L, *((double *) val));
            break;
        case OHM_TYPE_FLOAT:
            lua_pushnumber(L, *((float *) val));
            break;
        case OHM_TYPE_CHAR:
        case OHM_TYPE_UCHAR:
            lua_pushlstring(L, (const char *) val, type->nelem);
            break;
        case OHM_TYPE_LONG:
            lua_pushnumber(L, *((long *) val));
            break;
        case OHM_TYPE_ULONG:
        case OHM_TYPE_PTR:
            lua_pushnumber(L, *((unsigned long *) val));
            break;
        case OHM_TYPE_LONG_INT:
            lua_pushnumber(L, *((long int *) val));
            break;
        case OHM_TYPE_LONG_UINT:
            lua_pushnumber(L, *((long unsigned int *) val));
            break;
        default:
            derror("invalid probe data type (%d).", type->ohm_type);
            kill(0, SIGTERM);
    }
}
