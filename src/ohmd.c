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
#if HAVE_CMA && HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#if HAVE_MPI
#include "mpi.h"
#endif

#include "ohmd.h"

#if (!defined(PTRACE_PEEKUSER) && defined(PTRACE_PEEKUSR))
# define PTRACE_PEEKUSER PTRACE_PEEKUSR
#endif

#if (!defined(PTRACE_POKEUSER) && defined(PTRACE_POKEUSR))
# define PTRACE_POKEUSER PTRACE_POKEUSR
#endif

static double doctor_interval = DEFAULT_INTERVAL;
static bool   ohm_shutdown;
static pid_t  ohm_cpid;
int           ohm_debug;

// Global Lua state
static lua_State *L;

// Global unwind state
static unw_addr_space_t unw_addrspace;
static unw_cursor_t     unw_cursor;

int mpi_rank;
int mpi_size;

int cur_tick;

// Forward declaration
static addr_t _get_probe_var_addr(variable_t *var);

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

        traverse_die(cb, dbg, NULL, cu_die);
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
    char       *probe_name;

    probe_t    *p;

    np = 0;
    probe_initialize();
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
        probe_name = (char *) lua_tostring(L, -2);
        lua_pop(L, 1);

        p = new_probe(probe_name);
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

static void
usage(void)
{
    fprintf(stderr, "usage: " PACKAGE_NAME " [-D] [-o ohmfile]"
                    " [-i interval] <program> <args>\n\n");
    fprintf(stderr, "Report bugs to: " PACKAGE_BUGREPORT ".");
    exit(1);
}

static int
remote_copy(void *dst, void *src, size_t size, void *arg)
{
    int ret;
    // ddebug("mem read from %p to %p, size (%lu)", src, dst, size);

#if HAVE_CMA
    struct iovec local[1], remote[1];

    local[0].iov_base = dst;
    local[0].iov_len = size;
    remote[0].iov_base = src;
    remote[0].iov_len = size;
    USED(arg);
    ret = 0;
    do {
        ret += process_vm_readv(ohm_cpid, local, 1, remote, 1, 0);
    } while (ret > 0 && ret < size);
#elif HAVE_XPMEM
    USED(arg);
    ret = xpmem_copy(dst, src, size);
#else
    int i;
    for (i = 0; (i<<3) < size; i++)
        ret = _UPT_access_mem(unw_addrspace, (unw_word_t)((char*)src+(i<<3)),
                              (unw_word_t*)((char*)dst+(i<<3)), 0, arg);
#endif
    return ret;
}

#if 0
static void
push_lua(basetype_t *t, void *buf)
{
    int i;
    basetype_t *ot;

    if (is_array(t->ohm_type)) {
        for (i = 0; i < get_type_nelem(t); i++) {
            lua_newtable(L);

            ot = get_type_alias(t->elems[0]);
            if (get_type_nelem(ot) > 1)
                push_lua(ot, buf);
            else {
                lua_pushnumber(L, i+1);
                lua_pushbuf(L, ot, buf+(i*get_type_size(ot)));
                lua_rawset(L, -3);
            }
        }
    } else if (is_struct(t->ohm_type)) {
        for (i = 0; i < get_type_nelem(t); i++) {
            lua_newtable(L);

            ot = get_type_alias(t->elems[i]);
            if (get_type_nelem(ot) > 1)
                push_lua(ot, buf);
            else {
                ddebug("type: %s[%d] %lu (TID %d, type %d)::: %s",
                       ot->name, ot->nelem, ot->size, ot->id, ot->ohm_type,
                       t->elems[i]->name);
                lua_pushstring(L, t->elems[i]->name);
                lua_pushbuf(L, ot, buf+(i*get_type_size(ot)));
                lua_rawset(L, -3);
            }
        }
    } else
        lua_pushbuf(L, t, buf);
}
#endif

static int
write_lua(probe_t *probe, addr_t addr, void *arg)
{
    int ret, i, j;
    basetype_t *t, *ot;
    int nelem;
    size_t elem_size;
    addr_t iaddr;

    if (!probe)
        return -1;

    if (!is_builtin_probe(probe->type) && !probe->var)
        return -1;

    if (is_cur_tick(probe->type)) {
        memcpy(probe->buf, &cur_tick, sizeof(cur_tick));
    } else if (is_cur_frame(probe->type)) {
        printf("not implemented.");
    } else if (is_backtrace(probe->type)) {
        printf("not implemented.");
    } else if (is_ptr_addr(probe->type)) {
        memcpy(probe->buf, &addr, sizeof(addr));
    } else {
        t = get_type_alias(probe->var->type);
        size_t size = get_type_size(t);
        if (is_array(t->ohm_type)) {
            ot = get_type_alias(t->elems[0]);
            elem_size = get_type_size(ot);
            nelem = (probe->num < 0) ? (get_type_nelem(t)-probe->start) : probe->num+1;

            if (is_arr_ind(probe->type)) {
                if (probe->lower) {
                    iaddr = _get_probe_var_addr(probe->lower);
                    int start;
                    ret = remote_copy(&start, (void*)iaddr, sizeof(start), arg);
                    probe->start = start;
                    if (ret < 0)
                        return ret;
                }

                if (probe->upper) {
                    iaddr = _get_probe_var_addr(probe->upper);
                    int num;
                    ret = remote_copy(&num, (void*)iaddr, sizeof(num), arg);
                    probe->num = num;
                    if (ret < 0)
                        return ret;
                }

                nelem = (probe->num < 0) ? (get_type_nelem(t)-probe->start) : probe->num+1;
                if (size < ((probe->start+nelem) * elem_size)) {
                    ddebug("skipping probe %s: array index out of bounds",
                           probe->name);
                    return 1;
                } else {
                    addr = (addr_t)((char*)addr+(probe->start * elem_size));
                    size = nelem * elem_size;
                }
            }
        }
        ret = remote_copy(probe->buf, (void*)addr, size, arg);
        if (ret < 0)
            return ret;

        if (is_deref(probe->type)) {
            if ((t->ohm_type == OHM_TYPE_PTR) && (t->elems[0])) {
                addr = *(uintptr_t*)probe->buf;
                ret = remote_copy(probe->buf, (void*)addr, t->elems[0]->size, arg);
                if (ret < 0)
                    return ret;
            }
        } else if (is_struct_mem(probe->type)) {
            if ((t->ohm_type == OHM_TYPE_PTR) && (t->elems[0])) {
                addr = *(uintptr_t*)probe->buf + probe->start;
                ret = remote_copy(probe->buf, (void*)addr, probe->num, arg);
                if (ret < 0)
                    return ret;
            }
        }
    }

    lua_pushstring(L, probe->name);
    if ((is_array(t->ohm_type) && (nelem > 1))
        || is_struct(t->ohm_type))
        lua_newtable(L);

    size_t offset = 0;
    int start;
    if (is_array(t->ohm_type)) {
        if (is_arr_ind(probe->type)) {
            start = probe->start;
        } else {
            start = 0;
        }

        for (i = start, j = 1; i < start+nelem; i++, j++) {
            if (nelem > 1) {
                lua_pushnumber(L, j);
            }
            lua_pushbuf(L, ot, probe->buf+offset);
            offset += elem_size;

            if (nelem > 1) {
                lua_rawset(L, -3);
            }
        }
    } else if (is_struct(t->ohm_type)) {
        for (i = 0; i < get_type_nelem(t); i++) {
            ot = get_type_alias(t->elems[i]);
            lua_pushstring(L, t->elems[i]->name);
            lua_pushbuf(L, ot, probe->buf+offset);
            offset += t->elems[i]->size;
            lua_rawset(L, -3);
        }
    } else
        for (i = 0; i < get_type_nelem(t); i++) {
            lua_pushbuf(L, t, probe->buf+i);
        }

    lua_rawset(L, -3);
    return ret;
}

static addr_t
_get_probe_var_addr(variable_t *var) {
    unw_word_t ip, ptr;
    unw_cursor_t cur;

    if (!var) {
        return 0;
    }

    switch (var->loctype) {
        case OHM_ADDRESS:
            return var->addr;
            break;

        default:
            // copy the cursor so we can move back
            cur = unw_cursor;
            do {
                // unwind the stack to read as many probes as possible
                unw_get_reg(&cur, UNW_REG_IP, &ip);
                if (in_function(var->function, ip)) {
                    // Get the probe location
                    if (is_fbreg(var->loctype)) {
                        unw_get_reg(&cur, UNW_X86_64_RBP, &ptr);
                        ptr = ptr+16+var->offset;
                    } else if (is_reg(var->loctype)) {
                        unw_get_reg(&cur, var->offset, &ptr);
                    } else if (is_literal(var->loctype)) {
                        ptr = var->offset;
                    } else
                        derror("don't know how to read probe, skipping...");
                    // copy it to the Lua Land.
                    return ptr;
                }
            } while (!in_main(ip) && (unw_step(&cur) > 0));
            break;
    }
    return 0;
}


static void
probe(void *arg)
{
    probe_t *p;

    lua_getglobal(L, "ohm_add");
    if(!lua_isfunction(L, -1)) {
        lua_pop(L,1);
        return;
    }

    lua_newtable(L);
    for (p = probes_list; p != NULL; p = p->next) {
        addr_t addr = _get_probe_var_addr(p->var);
        if (!addr && !is_builtin_probe(p->type))
            continue;

        if (write_lua(p, addr, arg) < 0)
            derror("error in probe, skipping...");
    }

    if (lua_pcall(L, 1, 0, 0) != 0) {
        derror("error adding value: %s\n", lua_tostring(L, -1));
        return;
    }
    ++cur_tick;
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

    cur_tick = 0;

#ifdef HAVE_MPI
    int init;
    MPI_Initialized(&init);
    if (!init)
        MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
#else
    mpi_rank = 0;
    mpi_size = 0;
#endif      

    ohmfile = DEFAULT_OHMFILE;
    while ((c = getopt(argc, argv, "Do:i:h")) != -1) {
        switch (c) {
            case 'D':
                ohm_debug = (mpi_rank == 0);
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
    if ((ret = scan_file(argv[optind], &add_complextype_from_die)) < 0) {
        derror("error scanning types from %s. (compile with -g)",
               argv[optind]);
        goto error;
    }
    ddebug("%d base/complex types found.", types_table_size);
    // Since we do not topologically sort the DWARF graph, the
    // information of some of the array/struct types might be
    // incorrect. We fix them here.
    refresh_compound_sizes();

    // print the types table
    /* for (c = 0; c < types_table_size; c++) */
    /*     printf("%d :: %s[%d] %lu (TID %d)\n", c, types_table[c].name, */
    /*            types_table[c].nelem, types_table[c].size, */
    /*            types_table[c].id); */

    //  next we look for the variables.
    if ((ret = scan_file(argv[optind], &add_var_from_die)) < 0) {
        derror("error scanning variables from %s. (compile with -g)",
               argv[optind]);
        goto error;
    }
    ddebug("%d variables found.", vars_table_size);
    print_all_variables();
    ddebug("%d functions found.", fns_table_size);
    print_all_functions();

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
    signal(SIGSEGV, ohm_cleanup);

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

#ifdef HAVE_MPI
    int finalized;
    MPI_Finalized(&finalized);
    if (!finalized)
        MPI_Finalize();
#endif
    probe_finalize();
    return 0;

error:
    derror("Exiting doctor.");
    return -1;
}
