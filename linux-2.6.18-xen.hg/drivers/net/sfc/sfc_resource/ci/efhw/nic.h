/****************************************************************************
 * Driver for Solarflare network controllers -
 *          resource management for Xen backend, OpenOnload, etc
 *           (including support for SFE4001 10GBT NIC)
 *
 * This file contains API provided by efhw/nic.c file.  This file is not
 * designed for use outside of the SFC resource driver.
 *
 * Copyright 2005-2010: Solarflare Communications Inc,
 *                      9501 Jeronimo Road, Suite 250,
 *                      Irvine, CA 92618, USA
 *
 * Developed and maintained by Solarflare Communications:
 *                      <linux-xen-drivers@solarflare.com>
 *                      <onload-dev@solarflare.com>
 *
 * Certain parts of the driver were implemented by
 *          Alexandra Kossovsky <Alexandra.Kossovsky@oktetlabs.ru>
 *          OKTET Labs Ltd, Russia,
 *          http://oktetlabs.ru, <info@oktetlabs.ru>
 *          by request of Solarflare Communications
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************
 */

#ifndef __CI_EFHW_NIC_H__
#define __CI_EFHW_NIC_H__

#include <ci/efhw/efhw_types.h>
#include <ci/efhw/public.h>


/* Convert PCI info to device type.  Returns false when device is not
 * recognised.
 */
extern int efhw_device_type_init(struct efhw_device_type *dt,
				 int vendor_id, int device_id, int revision);

/* Initialise fields that do not involve touching hardware. */
extern void efhw_nic_init(struct efhw_nic *nic, unsigned flags,
			  unsigned options, struct efhw_device_type dev_type);

/*! Destruct NIC resources */
extern void efhw_nic_dtor(struct efhw_nic *nic);

/*! Shutdown interrupts */
extern void efhw_nic_close_interrupts(struct efhw_nic *nic);

#endif /* __CI_EFHW_NIC_H__ */
