/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef STAGE2_MM_H
#define STAGE2_MM_H

/**
 * @brief Get EPT pointer of the vm
 *
 * @param[in] vm the pointer that points to VM data structure
 *
 * @retval If the current context of vm is SECURE_WORLD, return EPT pointer of
 *            secure world, otherwise return EPT pointer of normal world.
 */
void *get_stg2ptp(struct acrn_vm *vm);

#endif /* STAGE2_MM_H */
