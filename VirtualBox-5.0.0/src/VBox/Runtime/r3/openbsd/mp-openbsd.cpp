/*
 * Copyright (C) 2008-2015 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_SYSTEM
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/sysctl.h>

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/log.h>
#include <iprt/once.h>
#include <iprt/critsect.h>


/**
 * Internal worker that determines the max possible CPU count.
 *
 * @returns Max cpus.
 */
static RTCPUID rtMpOpenBsdMaxCpus(void)
{
    int aiMib[2];
    aiMib[0] = CTL_HW;
    aiMib[1] = HW_NCPU;
    int cCpus = -1;
    size_t cb = sizeof(cCpus);
    int rc = sysctl(aiMib, RT_ELEMENTS(aiMib), &cCpus, &cb, NULL, 0);
    if (rc != -1 && cCpus >= 1)
        return cCpus;
    AssertFailed();
    return 1;
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < RTCPUSET_MAX_CPUS && idCpu < rtMpOpenBsdMaxCpus() ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < rtMpOpenBsdMaxCpus() ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return rtMpOpenBsdMaxCpus() - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    /*
     * OpenBSD doesn't support CPU hotplugging so every CPU which appears
     * in the tree is also online.
     */
    if (idCpu < rtMpOpenBsdMaxCpus())
 	return true;
   
    return false;
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    return idCpu != NIL_RTCPUID
        && idCpu < rtMpOpenBsdMaxCpus();
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCpuSetEmpty(pSet);
    RTCPUID cMax = rtMpOpenBsdMaxCpus();
    for (RTCPUID idCpu = 0; idCpu < cMax; idCpu++)
        if (RTMpIsCpuPossible(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    return rtMpOpenBsdMaxCpus();
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTCpuSetEmpty(pSet);
    RTCPUID cMax = rtMpOpenBsdMaxCpus();
    for (RTCPUID idCpu = 0; idCpu < cMax; idCpu++)
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    /*
     * OpenBSD has sysconf.
     */
    return sysconf(_SC_NPROCESSORS_ONLN);
}


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    int uFreqCurr = 0;
    size_t cbParameter = sizeof(uFreqCurr);
    int mib[2];

    if (!RTMpIsCpuOnline(idCpu))
        return 0;

    /* CPU's have a common frequency. */
    mib[0] = CTL_HW;
    mib[1] = HW_CPUSPEED;
    int rc = sysctl(mib, 2, &uFreqCurr, &cbParameter, NULL, 0);
    if (rc)
        return 0;
    
    return (uint32_t)uFreqCurr;
}


RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    int uFreq;
    int uPerf;
    size_t cbFreq = sizeof(uFreq);
    size_t cbPerf = sizeof(uPerf);
    uint32_t maxFreq;
    int mib[2];

    if (!RTMpIsCpuOnline(idCpu))
        return 0;

    /*
     * CPU 0 has the freq levels entry. ENOMEM is ok as we don't need all supported
     * levels but only the first one.
     */
    mib[0] = CTL_HW;
    mib[1] = HW_CPUSPEED;
    int rc = sysctl(mib, 2, &uFreq, &cbFreq, NULL, 0);
    if (   (rc && (errno != ENOMEM))
        || (uFreq == 0))
        return 0;

    mib[0] = CTL_HW;
    mib[1] = HW_SETPERF;
    rc = sysctl(mib, 2, &uPerf, &cbPerf, NULL, 0);
    if (   (rc && (errno != ENOMEM))
        || (uFreq == 0))
        return 0;

    maxFreq = uFreq / uPerf * 100;

    return maxFreq;
}

