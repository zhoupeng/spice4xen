/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 *          Tristan Gingold <tristan.gingold@bull.net>
 */
#include <xen/interface/xen-compat.h>
#define __XEN_TOOLS__
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/module.h>
#include <xen/interface/xen.h>
#include <xen/interface/platform.h>
#define __XEN__
#include <xen/interface/domctl.h>
#include <xen/interface/sysctl.h>
#include <xen/interface/memory.h>
#include <xen/interface/version.h>
#include <xen/interface/event_channel.h>
#include <xen/interface/xsm/acm_ops.h>
#include <xen/interface/hvm/params.h>
#include <xen/interface/arch-ia64/debug_op.h>
#include <xen/interface/tmem.h>
#include <xen/public/privcmd.h>
#include <asm/hypercall.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/xen/xencomm.h>

#define ROUND_DIV(v,s) (((v) + (s) - 1) / (s))

static int
xencomm_privcmd_platform_op(privcmd_hypercall_t *hypercall)
{
	struct xen_platform_op kern_op;
	struct xen_platform_op __user *user_op = (struct xen_platform_op __user *)hypercall->arg[0];
	struct xencomm_handle *op_desc;
	struct xencomm_handle *desc = NULL;
	int ret = 0;

	if (copy_from_user(&kern_op, user_op, sizeof(struct xen_platform_op)))
		return -EFAULT;

	if (kern_op.interface_version != XENPF_INTERFACE_VERSION)
		return -EACCES;

	op_desc = xencomm_map_no_alloc(&kern_op, sizeof(kern_op));

	switch (kern_op.cmd) {
	default:
		printk("%s: unknown platform cmd %d\n", __func__, kern_op.cmd);
		return -ENOSYS;
	}

	if (ret) {
		/* error mapping the nested pointer */
		return ret;
	}

	ret = xencomm_arch_hypercall_platform_op(op_desc);

	/* FIXME: should we restore the handle?  */
	if (copy_to_user(user_op, &kern_op, sizeof(struct xen_platform_op)))
		ret = -EFAULT;

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_sysctl(privcmd_hypercall_t *hypercall)
{
	xen_sysctl_t kern_op;
	xen_sysctl_t __user *user_op;
	struct xencomm_handle *op_desc;
	struct xencomm_handle *desc = NULL;
	struct xencomm_handle *desc1 = NULL;
	struct xencomm_handle *desc2 = NULL;
	int ret = 0;

	user_op = (xen_sysctl_t __user *)hypercall->arg[0];

	if (copy_from_user(&kern_op, user_op, sizeof(xen_sysctl_t)))
		return -EFAULT;

	if (kern_op.interface_version != XEN_SYSCTL_INTERFACE_VERSION)
		return -EACCES;

	op_desc = xencomm_map_no_alloc(&kern_op, sizeof(kern_op));

	switch (kern_op.cmd) {
	case XEN_SYSCTL_readconsole:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.readconsole.buffer),
			kern_op.u.readconsole.count);
		if (xen_guest_handle(kern_op.u.readconsole.buffer) != NULL &&
		    kern_op.u.readconsole.count > 0 && desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.readconsole.buffer,
		                     (void *)desc);
		break;
	case XEN_SYSCTL_tbuf_op:
	case XEN_SYSCTL_physinfo:
	case XEN_SYSCTL_sched_id:
	case XEN_SYSCTL_availheap:
		break;
	case XEN_SYSCTL_perfc_op:
	{
		struct xencomm_handle *tmp_desc;
		xen_sysctl_t tmp_op = {
			.cmd = XEN_SYSCTL_perfc_op,
			.interface_version = XEN_SYSCTL_INTERFACE_VERSION,
			.u.perfc_op = {
				.cmd = XEN_SYSCTL_PERFCOP_query,
				/* .desc.p = NULL, */
				/* .val.p = NULL, */
			},
		};

		if (xen_guest_handle(kern_op.u.perfc_op.desc) == NULL) {
			if (xen_guest_handle(kern_op.u.perfc_op.val) != NULL)
				return -EINVAL;
			break;
		}

		/* query the buffer size for xencomm */
		tmp_desc = xencomm_map_no_alloc(&tmp_op, sizeof(tmp_op));
		ret = xencomm_arch_hypercall_sysctl(tmp_desc);
		if (ret)
			return ret;

		desc = xencomm_map(xen_guest_handle(kern_op.u.perfc_op.desc),
				   tmp_op.u.perfc_op.nr_counters *
				   sizeof(xen_sysctl_perfc_desc_t));
		if (xen_guest_handle(kern_op.u.perfc_op.desc) != NULL &&
		    tmp_op.u.perfc_op.nr_counters > 0 && desc == NULL)
			return -ENOMEM;

		set_xen_guest_handle(kern_op.u.perfc_op.desc, (void *)desc);

		desc1 = xencomm_map(xen_guest_handle(kern_op.u.perfc_op.val),
				    tmp_op.u.perfc_op.nr_vals *
				    sizeof(xen_sysctl_perfc_val_t));
		if (xen_guest_handle(kern_op.u.perfc_op.val) != NULL &&
		    tmp_op.u.perfc_op.nr_vals > 0 && desc1 == NULL) {
			xencomm_free(desc);
			return -ENOMEM;
		}

		set_xen_guest_handle(kern_op.u.perfc_op.val, (void *)desc1);
		break;
	}
	case XEN_SYSCTL_getdomaininfolist:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.getdomaininfolist.buffer),
			kern_op.u.getdomaininfolist.max_domains *
			sizeof(xen_domctl_getdomaininfo_t));
		if (xen_guest_handle(kern_op.u.getdomaininfolist.buffer) !=
		    NULL && kern_op.u.getdomaininfolist.max_domains > 0 &&
		    desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.getdomaininfolist.buffer,
				     (void *)desc);
		break;
	case XEN_SYSCTL_debug_keys:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.debug_keys.keys),
			kern_op.u.debug_keys.nr_keys);
		if (xen_guest_handle(kern_op.u.debug_keys.keys) != NULL &&
		    kern_op.u.debug_keys.nr_keys > 0 && desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.debug_keys.keys,
				     (void *)desc);
		break;

	case XEN_SYSCTL_get_pmstat:
		if (kern_op.u.get_pmstat.type == PMSTAT_get_pxstat) {
			struct pm_px_stat *getpx =
				&kern_op.u.get_pmstat.u.getpx;
			desc = xencomm_map(
				xen_guest_handle(getpx->trans_pt),
				getpx->total * getpx->total *
				sizeof(uint64_t));
			if (xen_guest_handle(getpx->trans_pt) != NULL &&
			    getpx->total > 0 && desc == NULL)
				return -ENOMEM;

			set_xen_guest_handle(getpx->trans_pt, (void *)desc);

			desc1 = xencomm_map(xen_guest_handle(getpx->pt),
				getpx->total * sizeof(pm_px_val_t));
			if (xen_guest_handle(getpx->pt) != NULL &&
			    getpx->total > 0 && desc1 == NULL)
				return -ENOMEM;

			set_xen_guest_handle(getpx->pt, (void *)desc1);
		}
		break;

	case XEN_SYSCTL_topologyinfo:
	{
		xen_sysctl_topologyinfo_t *info = &kern_op.u.topologyinfo;
		unsigned long size =
			(info->max_cpu_index + 1) * sizeof(uint32_t);

		desc = xencomm_map(xen_guest_handle(info->cpu_to_core), size);
		if (xen_guest_handle(info->cpu_to_core) != NULL &&
		    info->max_cpu_index > 0 && desc == NULL)
			return -ENOMEM;

		set_xen_guest_handle(info->cpu_to_core, (void *)desc);

		desc1 = xencomm_map(
			xen_guest_handle(info->cpu_to_socket), size);
		if (xen_guest_handle(info->cpu_to_socket) != NULL &&
		    info->max_cpu_index > 0 && desc1 == NULL) {
			xencomm_free(desc);
			return -ENOMEM;
		}

		set_xen_guest_handle(info->cpu_to_socket, (void *)desc1);

		desc2 = xencomm_map(xen_guest_handle(info->cpu_to_node), size);
		if (xen_guest_handle(info->cpu_to_node) != NULL &&
		    info->max_cpu_index > 0 && desc2 == NULL) {
			xencomm_free(desc1);
			xencomm_free(desc);
			return -ENOMEM;
		}

		set_xen_guest_handle(info->cpu_to_node, (void *)desc2);
		break;
	}

	case XEN_SYSCTL_numainfo:
	{
		xen_sysctl_numainfo_t *info = &kern_op.u.numainfo;
		uint32_t max = info->max_node_index;

		desc = xencomm_map(xen_guest_handle(info->node_to_memsize),
				   (max + 1) * sizeof(uint64_t));
		if (xen_guest_handle(info->node_to_memsize) != NULL &&
		    desc == NULL)
			return -ENOMEM;

		set_xen_guest_handle(info->node_to_memsize, (void *)desc);

		desc1 = xencomm_map(xen_guest_handle(info->node_to_memfree),
				    (max + 1) * sizeof(uint64_t));
		if (xen_guest_handle(info->node_to_memfree) != NULL &&
		    desc1 == NULL) {
			xencomm_free(desc);
			return -ENOMEM;
		}

		set_xen_guest_handle(info->node_to_memfree, (void *)desc1);

		desc2 = xencomm_map(
			xen_guest_handle(info->node_to_node_distance),
			(max + 1) * (max + 1) * sizeof(uint32_t));
		if (xen_guest_handle(info->node_to_node_distance) != NULL &&
		    desc2 == NULL) {
			xencomm_free(desc1);
			xencomm_free(desc);
			return -ENOMEM;
		}

		set_xen_guest_handle(info->node_to_node_distance,
				     (void *)desc2);
		break;
	}

	case XEN_SYSCTL_cpupool_op:
		if (kern_op.u.cpupool_op.op != XEN_SYSCTL_CPUPOOL_OP_INFO &&
		    kern_op.u.cpupool_op.op != XEN_SYSCTL_CPUPOOL_OP_FREEINFO)
			break;
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.cpupool_op.cpumap.bitmap),
			ROUND_DIV(kern_op.u.cpupool_op.cpumap.nr_cpus, 8));
		if (xen_guest_handle(kern_op.u.cpupool_op.cpumap.bitmap) !=
		    NULL && kern_op.u.cpupool_op.cpumap.nr_cpus > 0 &&
		    desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.cpupool_op.cpumap.bitmap,
		                     (void *)desc);
		break;

	default:
		printk("%s: unknown sysctl cmd %d\n", __func__, kern_op.cmd);
		return -ENOSYS;
	}

	if (ret) {
		/* error mapping the nested pointer */
		return ret;
	}

	ret = xencomm_arch_hypercall_sysctl(op_desc);

	/* FIXME: should we restore the handles?  */
	if (copy_to_user(user_op, &kern_op, sizeof(xen_sysctl_t)))
		ret = -EFAULT;

	xencomm_free(desc);
	xencomm_free(desc1);
	xencomm_free(desc2);
	return ret;
}

static int
xencomm_privcmd_domctl(privcmd_hypercall_t *hypercall)
{
	xen_domctl_t kern_op;
	xen_domctl_t __user *user_op;
	struct xencomm_handle *op_desc;
	struct xencomm_handle *desc = NULL;
	int ret = 0;

	user_op = (xen_domctl_t __user *)hypercall->arg[0];

	if (copy_from_user(&kern_op, user_op, sizeof(xen_domctl_t)))
		return -EFAULT;

	if (kern_op.interface_version != XEN_DOMCTL_INTERFACE_VERSION)
		return -EACCES;

	op_desc = xencomm_map_no_alloc(&kern_op, sizeof(kern_op));

	switch (kern_op.cmd) {
	case XEN_DOMCTL_createdomain:
	case XEN_DOMCTL_destroydomain:
	case XEN_DOMCTL_pausedomain:
	case XEN_DOMCTL_unpausedomain:
	case XEN_DOMCTL_resumedomain:
	case XEN_DOMCTL_getdomaininfo:
		break;
	case XEN_DOMCTL_getmemlist:
	{
		unsigned long nr_pages = kern_op.u.getmemlist.max_pfns;

		desc = xencomm_map(
			xen_guest_handle(kern_op.u.getmemlist.buffer),
			nr_pages * sizeof(unsigned long));
		if (xen_guest_handle(kern_op.u.getmemlist.buffer) != NULL &&
		    nr_pages > 0 && desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.getmemlist.buffer,
		                     (void *)desc);
		break;
	}
	case XEN_DOMCTL_getpageframeinfo:
		break;
	case XEN_DOMCTL_getpageframeinfo2:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.getpageframeinfo2.array),
			kern_op.u.getpageframeinfo2.num);
		if (xen_guest_handle(kern_op.u.getpageframeinfo2.array) !=
		    NULL && kern_op.u.getpageframeinfo2.num > 0 &&
		    desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.getpageframeinfo2.array,
		                     (void *)desc);
		break;
	case XEN_DOMCTL_shadow_op:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.shadow_op.dirty_bitmap),
			ROUND_DIV(kern_op.u.shadow_op.pages, 8));
		if (xen_guest_handle(kern_op.u.shadow_op.dirty_bitmap) != NULL
		    && kern_op.u.shadow_op.pages > 0 && desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.shadow_op.dirty_bitmap,
		                     (void *)desc);
		break;
	case XEN_DOMCTL_max_mem:
		break;
	case XEN_DOMCTL_setvcpucontext:
	case XEN_DOMCTL_getvcpucontext:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.vcpucontext.ctxt),
			sizeof(vcpu_guest_context_t));
		if (xen_guest_handle(kern_op.u.vcpucontext.ctxt) != NULL &&
		    desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.vcpucontext.ctxt, (void *)desc);
		break;
	case XEN_DOMCTL_getvcpuinfo:
		break;
	case XEN_DOMCTL_setvcpuaffinity:
	case XEN_DOMCTL_getvcpuaffinity:
		desc = xencomm_map(
			xen_guest_handle(kern_op.u.vcpuaffinity.cpumap.bitmap),
			ROUND_DIV(kern_op.u.vcpuaffinity.cpumap.nr_cpus, 8));
		if (xen_guest_handle(kern_op.u.vcpuaffinity.cpumap.bitmap) !=
		    NULL && kern_op.u.vcpuaffinity.cpumap.nr_cpus > 0 &&
		    desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.vcpuaffinity.cpumap.bitmap,
		                     (void *)desc);
		break;
	case XEN_DOMCTL_gethvmcontext:
	case XEN_DOMCTL_sethvmcontext:
		if (kern_op.u.hvmcontext.size > 0)
			desc = xencomm_map(
				xen_guest_handle(kern_op.u.hvmcontext.buffer),
				kern_op.u.hvmcontext.size);
		if (xen_guest_handle(kern_op.u.hvmcontext.buffer) != NULL &&
		    kern_op.u.hvmcontext.size > 0 && desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.hvmcontext.buffer, (void*)desc);
		break;
	case XEN_DOMCTL_get_device_group: 
	{
		struct xen_domctl_get_device_group *get_device_group =
			&kern_op.u.get_device_group;
		desc = xencomm_map(
			xen_guest_handle(get_device_group->sdev_array),
			get_device_group->max_sdevs * sizeof(uint32_t));
		if (xen_guest_handle(get_device_group->sdev_array) != NULL &&
		    get_device_group->max_sdevs > 0 && desc == NULL)
			return -ENOMEM;
		set_xen_guest_handle(kern_op.u.get_device_group.sdev_array,
				     (void*)desc);
		break;
	}
	case XEN_DOMCTL_max_vcpus:
	case XEN_DOMCTL_scheduler_op:
	case XEN_DOMCTL_setdomainhandle:
	case XEN_DOMCTL_setdebugging:
	case XEN_DOMCTL_irq_permission:
	case XEN_DOMCTL_iomem_permission:
	case XEN_DOMCTL_ioport_permission:
	case XEN_DOMCTL_hypercall_init:
	case XEN_DOMCTL_arch_setup:
	case XEN_DOMCTL_settimeoffset:
	case XEN_DOMCTL_sendtrigger:
	case XEN_DOMCTL_set_opt_feature:
	case XEN_DOMCTL_assign_device:
	case XEN_DOMCTL_subscribe:
	case XEN_DOMCTL_test_assign_device:
	case XEN_DOMCTL_deassign_device:
	case XEN_DOMCTL_bind_pt_irq:
	case XEN_DOMCTL_unbind_pt_irq:
	case XEN_DOMCTL_memory_mapping:
	case XEN_DOMCTL_ioport_mapping:
	case XEN_DOMCTL_set_address_size:
	case XEN_DOMCTL_get_address_size:
	case XEN_DOMCTL_mem_sharing_op:
		break;
	case XEN_DOMCTL_pin_mem_cacheattr:
		return -ENOSYS;
	default:
		printk("%s: unknown domctl cmd %d\n", __func__, kern_op.cmd);
		return -ENOSYS;
	}

	if (ret) {
		/* error mapping the nested pointer */
		return ret;
	}

	ret = xencomm_arch_hypercall_domctl (op_desc);

	/* FIXME: should we restore the handle?  */
	if (copy_to_user(user_op, &kern_op, sizeof(xen_domctl_t)))
		ret = -EFAULT;

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_xsm_op(privcmd_hypercall_t *hypercall)
{
	void __user *arg = (void __user *)hypercall->arg[0];
	xen_acmctl_t kern_arg;
	struct xencomm_handle *op_desc;
	struct xencomm_handle *desc = NULL;
	int ret;

	if (copy_from_user(&kern_arg, arg, sizeof(kern_arg)))
		return -EFAULT;
	if (kern_arg.interface_version != ACM_INTERFACE_VERSION)
		return -ENOSYS;
	
	switch (kern_arg.cmd) {
	case ACMOP_getssid: {
		op_desc = xencomm_map_no_alloc(&kern_arg, sizeof(kern_arg));

		desc = xencomm_map(
			xen_guest_handle(kern_arg.u.getssid.ssidbuf),
			kern_arg.u.getssid.ssidbuf_size);
		if (xen_guest_handle(kern_arg.u.getssid.ssidbuf) != NULL &&
		    kern_arg.u.getssid.ssidbuf_size > 0 && desc == NULL)
			return -ENOMEM;

		set_xen_guest_handle(kern_arg.u.getssid.ssidbuf, (void *)desc);

		ret = xencomm_arch_hypercall_xsm_op(op_desc);

		xencomm_free(desc);

		if (copy_to_user(arg, &kern_arg, sizeof(kern_arg)))
			return -EFAULT;
		return ret;
	}
	default:
		printk("%s: unknown acm_op cmd %d\n", __func__, kern_arg.cmd);
		return -ENOSYS;
	}

	return ret;
}

static int
xencomm_privcmd_memory_reservation_op(privcmd_hypercall_t *hypercall)
{
	const unsigned long cmd = hypercall->arg[0];
	int ret = 0;
	xen_memory_reservation_t kern_op;
	xen_memory_reservation_t __user *user_op;
	struct xencomm_handle *desc = NULL;
	struct xencomm_handle *desc_op;

	user_op = (xen_memory_reservation_t __user *)hypercall->arg[1];
	if (copy_from_user(&kern_op, user_op,
			   sizeof(xen_memory_reservation_t)))
		return -EFAULT;
	desc_op = xencomm_map_no_alloc(&kern_op, sizeof(kern_op));

	if (!xen_guest_handle(kern_op.extent_start)) {
		ret = xencomm_arch_hypercall_memory_op(cmd, desc_op);
		if (ret < 0)
			return ret;
	} else {
		xen_ulong_t nr_done = 0;
		xen_ulong_t nr_extents = kern_op.nr_extents;
		void *addr = xen_guest_handle(kern_op.extent_start);
			
		/*
		 * Work around.
		 *   Xencomm has single page size limit caused
		 *   by xencomm_alloc()/xencomm_free() so that
		 *   we have to repeat the hypercall.
		 *   This limitation can be removed.
		 */
#define MEMORYOP_XENCOMM_LIMIT						\
		(((((PAGE_SIZE - sizeof(struct xencomm_desc)) /		\
		    sizeof(uint64_t)) - 2) * PAGE_SIZE) /		\
		 sizeof(*xen_guest_handle(kern_op.extent_start)))

		/*
		 * Work around.
		 *   Even if the above limitation is removed,
		 *   the hypercall with large number of extents 
		 *   may cause the soft lockup warning.
		 *   In order to avoid the warning, we limit
		 *   the number of extents and repeat the hypercall.
		 *   The following value is determined by evaluation.
		 *   Time of one hypercall should be smaller than
		 *   a vcpu time slice. The time with current
		 *   MEMORYOP_MAX_EXTENTS is around 5 msec.
		 *   If the following limit causes some issues,
		 *   we should decrease this value.
		 *
		 *   Another way would be that start with small value and
		 *   increase adoptively measuring hypercall time.
		 *   It might be over-kill.
		 */
#define MEMORYOP_MAX_EXTENTS	(MEMORYOP_XENCOMM_LIMIT / 512)

		while (nr_extents > 0) {
			xen_ulong_t nr_tmp = nr_extents;
			if (nr_tmp > MEMORYOP_MAX_EXTENTS)
				nr_tmp = MEMORYOP_MAX_EXTENTS;

			kern_op.nr_extents = nr_tmp;
			desc = xencomm_map
				(addr + nr_done * sizeof(*xen_guest_handle(kern_op.extent_start)),
				 nr_tmp * sizeof(*xen_guest_handle(kern_op.extent_start)));
			if (addr != NULL && nr_tmp > 0 && desc == NULL)
				return nr_done > 0 ? nr_done : -ENOMEM;

			set_xen_guest_handle(kern_op.extent_start,
					     (void *)desc);

			ret = xencomm_arch_hypercall_memory_op(cmd, desc_op);
			xencomm_free(desc);
			if (ret < 0)
				return nr_done > 0 ? nr_done : ret;

			nr_done += ret;
			nr_extents -= ret;
			if (ret < nr_tmp)
				break;

			/*
			 * prevent softlock up message.
			 * give cpu to soft lockup kernel thread.
			 */
			if (nr_extents > 0)
				schedule();
		}
		ret = nr_done;
		set_xen_guest_handle(kern_op.extent_start, addr);
	}

	if (copy_to_user(user_op, &kern_op, sizeof(xen_memory_reservation_t)))
		return -EFAULT;

	return ret;
}

static int
xencomm_privcmd_memory_op(privcmd_hypercall_t *hypercall)
{
	const unsigned long cmd = hypercall->arg[0];
	int ret = 0;

	switch (cmd) {
	case XENMEM_increase_reservation:
	case XENMEM_decrease_reservation:
	case XENMEM_populate_physmap:
		return xencomm_privcmd_memory_reservation_op(hypercall);
	case XENMEM_maximum_gpfn:
	{
		domid_t kern_domid;
		domid_t __user *user_domid;
		struct xencomm_handle *desc;

		user_domid = (domid_t __user *)hypercall->arg[1];
		if (copy_from_user(&kern_domid, user_domid, sizeof(domid_t)))
			return -EFAULT;
		desc = xencomm_map_no_alloc(&kern_domid, sizeof(kern_domid));

		ret = xencomm_arch_hypercall_memory_op(cmd, desc);

		return ret;
	}
	case XENMEM_add_to_physmap:
	{
		void __user *arg = (void __user *)hypercall->arg[1];
		struct xencomm_handle *desc;

		desc = xencomm_map(arg, sizeof(struct xen_add_to_physmap));
		if (desc == NULL)
			return -ENOMEM;

		ret = xencomm_arch_hypercall_memory_op(cmd, desc);

		xencomm_free(desc);
		return ret;
	}
	default:
		printk("%s: unknown memory op %lu\n", __func__, cmd);
		ret = -ENOSYS;
	}
	return ret;
}

static int
xencomm_privcmd_xen_version(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	void __user *arg = (void __user *)hypercall->arg[1];
	struct xencomm_handle *desc;
	size_t argsize;
	int rc;

	switch (cmd) {
	case XENVER_version:
		/* do not actually pass an argument */
		return xencomm_arch_hypercall_xen_version(cmd, 0);
	case XENVER_extraversion:
		argsize = sizeof(xen_extraversion_t);
		break;
	case XENVER_compile_info:
		argsize = sizeof(xen_compile_info_t);
		break;
	case XENVER_capabilities:
		argsize = sizeof(xen_capabilities_info_t);
		break;
	case XENVER_changeset:
		argsize = sizeof(xen_changeset_info_t);
		break;
	case XENVER_platform_parameters:
		argsize = sizeof(xen_platform_parameters_t);
		break;
	case XENVER_pagesize:
		argsize = (arg == NULL) ? 0 : sizeof(void *);
		break;
	case XENVER_get_features:
		argsize = (arg == NULL) ? 0 : sizeof(xen_feature_info_t);
		break;
	case XENVER_commandline:
		argsize = sizeof(xen_commandline_t);
		break;

	default:
		printk("%s: unknown version op %d\n", __func__, cmd);
		return -ENOSYS;
	}

	desc = xencomm_map(arg, argsize);
	if (arg != NULL && argsize > 0 && desc == NULL)
		return -ENOMEM;

	rc = xencomm_arch_hypercall_xen_version(cmd, desc);

	xencomm_free(desc);

	return rc;
}

static int
xencomm_privcmd_event_channel_op(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	struct xencomm_handle *desc;
	unsigned int argsize;
	int ret;

	switch (cmd) {
	case EVTCHNOP_alloc_unbound:
		argsize = sizeof(evtchn_alloc_unbound_t);
		break;

	case EVTCHNOP_status:
		argsize = sizeof(evtchn_status_t);
		break;

	default:
		printk("%s: unknown EVTCHNOP %d\n", __func__, cmd);
		return -EINVAL;
	}

	desc = xencomm_map((void *)hypercall->arg[1], argsize);
	if ((void *)hypercall->arg[1] != NULL && argsize > 0 && desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_event_channel_op(cmd, desc);

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_hvm_op_track_dirty_vram(privcmd_hypercall_t *hypercall)
{
#if 1
	/*
	 * At this moment HVMOP_track_dirty_vram isn't implemented
	 * on xen/ia64 so that it just returns -ENOSYS.
	 * Don't issue hypercall to get -ENOSYS.
	 * When the hypercall is implemented, enable the following codes.
	 */
	return -ENOSYS;
#else
	int cmd = hypercall->arg[0];
	struct xen_hvm_track_dirty_vram *user_op = (void*)hypercall->arg[1];
	struct xen_hvm_track_dirty_vram kern_op;
	struct xencomm_handle *desc;
	struct xencomm_handle *bitmap_desc;
	int ret;

	BUG_ON(cmd != HVMOP_track_dirty_vram);
	if (copy_from_user(&kern_op, user_op, sizeof(kern_op)))
		return -EFAULT;
	desc = xencomm_map_no_alloc(&kern_op, sizeof(kern_op));
	bitmap_desc = xencomm_map(xen_guest_handle(kern_op.dirty_bitmap),
				  kern_op.nr * sizeof(uint8_t));
	if (bitmap_desc == NULL)
		return -ENOMEM;
	set_xen_guest_handle(kern_op.dirty_bitmap, (void*)bitmap_desc);
	ret = xencomm_arch_hypercall_hvm_op(cmd, desc);
	xencomm_free(bitmap_desc);

	return ret;
#endif
}

static int
xencomm_privcmd_hvm_op(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	struct xencomm_handle *desc;
	unsigned int argsize;
	int ret;

	switch (cmd) {
	case HVMOP_get_param:
	case HVMOP_set_param:
		argsize = sizeof(xen_hvm_param_t);
		break;
	case HVMOP_set_pci_intx_level:
		argsize = sizeof(xen_hvm_set_pci_intx_level_t);
		break;
	case HVMOP_set_isa_irq_level:
		argsize = sizeof(xen_hvm_set_isa_irq_level_t);
		break;
	case HVMOP_set_pci_link_route:
		argsize = sizeof(xen_hvm_set_pci_link_route_t);
		break;
	case HVMOP_set_mem_type:
		argsize = sizeof(xen_hvm_set_mem_type_t);
		break;

	case HVMOP_track_dirty_vram:
		return xencomm_privcmd_hvm_op_track_dirty_vram(hypercall);

	default:
		printk("%s: unknown HVMOP %d\n", __func__, cmd);
		return -EINVAL;
	}

	desc = xencomm_map((void *)hypercall->arg[1], argsize);
	if ((void *)hypercall->arg[1] != NULL && argsize > 0 && desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_hvm_op(cmd, desc);

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_sched_op(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	struct xencomm_handle *desc;
	unsigned int argsize;
	int ret;

	switch (cmd) {
	case SCHEDOP_remote_shutdown:
		argsize = sizeof(sched_remote_shutdown_t);
		break;
	default:
		printk("%s: unknown SCHEDOP %d\n", __func__, cmd);
		return -EINVAL;
	}

	desc = xencomm_map((void *)hypercall->arg[1], argsize);
	if ((void *)hypercall->arg[1] != NULL && argsize > 0 && desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_sched_op(cmd, desc);

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_dom0vp_get_memmap(domid_t domid,
				  char* __user buf, unsigned long bufsize)
{
	int ret;
	struct xencomm_handle *desc;

	desc = xencomm_map(buf, bufsize);
	if (bufsize > 0 && desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_get_memmap((domid_t)domid, desc);

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_ia64_dom0vp_op(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	int ret;

	switch (cmd) {
	case IA64_DOM0VP_fpswa_revision: {
		unsigned int revision;
		unsigned int __user *revision_user =
			(unsigned int* __user)hypercall->arg[1];
		struct xencomm_handle *desc;
		desc = xencomm_map(&revision, sizeof(revision));
		if (desc == NULL)
			return -ENOMEM;

		ret = xencomm_arch_hypercall_fpswa_revision(desc);
		xencomm_free(desc);
		if (ret)
			break;
		if (copy_to_user(revision_user, &revision, sizeof(revision)))
			ret = -EFAULT;
		break;
	}
#ifdef CONFIG_XEN_IA64_EXPOSE_P2M
	case IA64_DOM0VP_expose_foreign_p2m:
		ret = xen_foreign_p2m_expose(hypercall);
		break;
#endif
	case IA64_DOM0VP_get_memmap:
		ret = xencomm_privcmd_dom0vp_get_memmap(
			(domid_t)hypercall->arg[1],
			(char* __user)hypercall->arg[2], hypercall->arg[3]);
		break;
	default:
		printk("%s: unknown IA64 DOM0VP op %d\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
xencomm_privcmd_ia64_debug_op(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	unsigned long domain = hypercall->arg[1];
	struct xencomm_handle *desc;
	int ret;

	switch (cmd) {
	case XEN_IA64_DEBUG_OP_SET_FLAGS:
	case XEN_IA64_DEBUG_OP_GET_FLAGS:
		break;
	default:
		printk("%s: unknown IA64 DEBUGOP %d\n", __func__, cmd);
		return -EINVAL;
	}

	desc = xencomm_map((void *)hypercall->arg[2],
			   sizeof(xen_ia64_debug_op_t));
	if (desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_ia64_debug_op(cmd, domain, desc);

	xencomm_free(desc);
	return ret;	
}

static int
xencomm_privcmd_ia64_physdev_op(privcmd_hypercall_t *hypercall)
{
	int cmd = hypercall->arg[0];
	struct xencomm_handle *desc;
	unsigned int argsize;
	int ret;

	switch (cmd) {
	case PHYSDEVOP_map_pirq:
		argsize = sizeof(physdev_map_pirq_t);
		break;
	case PHYSDEVOP_unmap_pirq:
		argsize = sizeof(physdev_unmap_pirq_t);
		break;
	default:
		printk("%s: unknown PHYSDEVOP %d\n", __func__, cmd);
		return -EINVAL;
	}

	desc = xencomm_map((void *)hypercall->arg[1], argsize);
	if ((void *)hypercall->arg[1] != NULL && argsize > 0 && desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_physdev_op(cmd, desc);

	xencomm_free(desc);
	return ret;
}

static int
xencomm_privcmd_tmem_op(privcmd_hypercall_t *hypercall)
{
	struct xencomm_handle *desc;
	int ret;

	desc = xencomm_map((void *)hypercall->arg[0], sizeof(struct tmem_op));
	if (desc == NULL)
		return -ENOMEM;

	ret = xencomm_arch_hypercall_tmem_op(desc);

	xencomm_free(desc);
	return ret;
}

int
privcmd_hypercall(privcmd_hypercall_t *hypercall)
{
	switch (hypercall->op) {
	case __HYPERVISOR_platform_op:
		return xencomm_privcmd_platform_op(hypercall);
	case __HYPERVISOR_domctl:
		return xencomm_privcmd_domctl(hypercall);
	case __HYPERVISOR_sysctl:
		return xencomm_privcmd_sysctl(hypercall);
	case __HYPERVISOR_xsm_op:
		return xencomm_privcmd_xsm_op(hypercall);
	case __HYPERVISOR_xen_version:
		return xencomm_privcmd_xen_version(hypercall);
	case __HYPERVISOR_memory_op:
		return xencomm_privcmd_memory_op(hypercall);
	case __HYPERVISOR_event_channel_op:
		return xencomm_privcmd_event_channel_op(hypercall);
	case __HYPERVISOR_hvm_op:
		return xencomm_privcmd_hvm_op(hypercall);
	case __HYPERVISOR_sched_op:
		return xencomm_privcmd_sched_op(hypercall);
	case __HYPERVISOR_ia64_dom0vp_op:
		return xencomm_privcmd_ia64_dom0vp_op(hypercall);
	case __HYPERVISOR_ia64_debug_op:
		return xencomm_privcmd_ia64_debug_op(hypercall);
	case __HYPERVISOR_physdev_op:
		return xencomm_privcmd_ia64_physdev_op(hypercall);
	case __HYPERVISOR_tmem_op:
		return xencomm_privcmd_tmem_op(hypercall);
	default:
		printk("%s: unknown hcall (%ld)\n", __func__, hypercall->op);
		return -ENOSYS;
	}
}

