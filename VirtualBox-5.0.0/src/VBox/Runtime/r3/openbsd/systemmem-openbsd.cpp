/*
 * Copyright (C) 2012-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>


RTDECL(int) RTSystemQueryTotalRam(uint64_t *pcb)
{
    int rc = VINF_SUCCESS;
    u_long cbMemPhys = 0;
    size_t cbParameter = sizeof(cbMemPhys);
    int mib[2];

    AssertPtrReturn(pcb, VERR_INVALID_POINTER);

    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM64;
    if(!sysctl(mib, 2, &cbMemPhys, &cbParameter, NULL, 0))
    {
        *pcb = cbMemPhys;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTDECL(int) RTSystemQueryAvailableRam(uint64_t *pcb)
{
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    struct uvmexp uvmexp;
    struct bcachestats bcstats;
    u_int cPagesMemFree = 0;
    u_int cPagesMemInactive = 0;
    u_int cPagesMemCached = 0;
    int cbPage = 0;
    size_t cbParameter;
    int cProcessed = 0;
    int mib[3];

    mib[0] = CTL_VM;
    mib[1] = VM_UVMEXP;
    cbParameter = sizeof(uvmexp);
    if (sysctl(mib, 2, &uvmexp, &cbParameter, NULL, 0))
        rc = RTErrConvertFromErrno(errno);
    cPagesMemFree = uvmexp.free;
    cPagesMemInactive = uvmexp.inactive;

    mib[0] = CTL_VFS;
    mib[1] = VFS_GENERIC;
    mib[2] = VFS_BCACHESTAT;
    cbParameter = sizeof(bcstats);
    if (   RT_SUCCESS(rc)
	   && sysctl(mib, 3, &bcstats, &cbParameter, NULL, 0))
        rc = RTErrConvertFromErrno(errno);
    cPagesMemCached = bcstats.numbufpages;

    mib[0] = CTL_HW;
    mib[1] = HW_PAGESIZE;
    cbParameter = sizeof(cbPage);
    if (   RT_SUCCESS(rc)
	   && sysctl(mib, 2, &cbPage, &cbParameter, NULL, 0))
        rc = RTErrConvertFromErrno(errno);

    if (RT_SUCCESS(rc))
        *pcb = (cPagesMemFree + cPagesMemInactive + cPagesMemCached ) * cbPage;

    return rc;
}

