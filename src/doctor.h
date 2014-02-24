// Copyright (c) 2010-2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#ifndef _DOCTOR_H
#define _DOCTOR_H

#include <stdio.h>

#define DEFAULT_OHMFILE         "default.ohm"
#define DEFAULT_INTERVAL        3.0
#define DEFAULT_NUM_TYPES       64
#define DEFAULT_NUM_FUNCTIONS   256
#define DEFAULT_NUM_VARS        256


typedef struct basetypes_t basetypes_t;
struct basetypes_t
{
    int          id;
    char         name[64];
    size_t       size;
    unsigned int nelem;
};


typedef struct functions_t functions_t;
struct functions_t
{
    char          name[128];
    unsigned long lowpc;
    unsigned long hipc;
};


typedef struct variables_t variables_t;
struct variables_t
{
    char          name[256];
    basetypes_t  *type;
    functions_t  *function;
    unsigned int  global;
    unsigned long addr;
    signed long   frame_offset;
};


typedef struct probe_t probe_t;
struct probe_t
{
    variables_t *var;
    probe_t     *next;
};

#define OHM_COLOR_ERROR "\x1b[31m"
#define OHM_COLOR_DEBUG "\x1b[32m"
#define OHM_COLOR_RESET "\x1b[0m"

#define derror(format, args...) do {                               \
    fprintf(stderr, OHM_COLOR_ERROR "[ERROR] " OHM_COLOR_RESET     \
            "%s:%d: " format "\n",                                 \
            __func__, __LINE__, ##args); } while (0)

#define ddebug(format, args...) do {                               \
    if (doctor_debug)                                              \
        fprintf(stderr, OHM_COLOR_DEBUG "[DEBUG] " OHM_COLOR_RESET \
                "%s:%d: " format "\n",                             \
                __func__, __LINE__, ##args); } while (0)


#endif /* _DOCTOR_H */
