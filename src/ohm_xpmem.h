// Copyright (c) 2014, Abhishek Kulkarni
// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

#ifndef _OHM_XPMEM_H
#define _OHM_XPMEM_H

#define XPMEM_MAX_SEGS 128

// Initialize OHM XPMEM support.

// This registers the stack and heap regions of the currently running
// process with XPMEM.
int ohm_xpmem_init(void);

// Finalize OHM XPMEM.
void ohm_xpmem_finalize(void);

#endif /* _OHM_XPMEM_H */
