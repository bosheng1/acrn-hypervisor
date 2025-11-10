/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef S2PT_H
#define S2PT_H

static inline void flush_guest_gtlb_local(void)           \
{                                       \
        asm volatile(                   \
                "hfence.gvma;"          \
                : : : "memory");        \
}



#endif /* S2PT_H */

