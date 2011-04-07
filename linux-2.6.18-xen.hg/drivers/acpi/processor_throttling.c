/*
 * processor_throttling.c - Throttling submodule of the ACPI processor driver
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2004       Dominik Brodowski <linux@brodo.de>
 *  Copyright (C) 2004  Anil S Keshavamurthy <anil.s.keshavamurthy@intel.com>
 *  			- Added processor hotplug support
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <acpi/acpi_bus.h>
#include <acpi/processor.h>

#define ACPI_PROCESSOR_COMPONENT        0x01000000
#define ACPI_PROCESSOR_CLASS            "processor"
#define _COMPONENT              ACPI_PROCESSOR_COMPONENT
ACPI_MODULE_NAME("processor_throttling");

struct throttling_tstate {
	unsigned int cpu;		/* cpu nr */
	int target_state;		/* target T-state */
};

#define THROTTLING_PRECHANGE       (1)
#define THROTTLING_POSTCHANGE      (2)

static int acpi_processor_get_throttling(struct acpi_processor *pr);
int acpi_processor_set_throttling(struct acpi_processor *pr, int state);

static int acpi_processor_update_tsd_coord(void)
{
	int count, count_target;
	int retval = 0;
	unsigned int i, j;
	cpumask_t covered_cpus;
	struct acpi_processor *pr, *match_pr;
	struct acpi_tsd_package *pdomain, *match_pdomain;
	struct acpi_processor_throttling *pthrottling, *match_pthrottling;

	/*
	 * Now that we have _TSD data from all CPUs, lets setup T-state
	 * coordination between all CPUs.
	 */
	for_each_possible_cpu(i) {
		pr = processors[i];
		if (!pr)
			continue;

		/* Basic validity check for domain info */
		pthrottling = &(pr->throttling);

		/*
		 * If tsd package for one cpu is invalid, the coordination
		 * among all CPUs is thought as invalid.
		 * Maybe it is ugly.
		 */
		if (!pthrottling->tsd_valid_flag) {
			retval = -EINVAL;
			break;
		}
	}
	if (retval)
		goto err_ret;

	cpus_clear(covered_cpus);
	for_each_possible_cpu(i) {
		pr = processors[i];
		if (!pr)
			continue;

		if (cpu_isset(i, covered_cpus))
			continue;
		pthrottling = &pr->throttling;

		pdomain = &(pthrottling->domain_info);
		cpu_set(i, pthrottling->shared_cpu_map);
		cpu_set(i, covered_cpus);
		/*
		 * If the number of processor in the TSD domain is 1, it is
		 * unnecessary to parse the coordination for this CPU.
		 */
		if (pdomain->num_processors <= 1)
			continue;

		/* Validate the Domain info */
		count_target = pdomain->num_processors;
		count = 1;

		for_each_possible_cpu(j) {
			if (i == j)
				continue;

			match_pr = processors[j];
			if (!match_pr)
				continue;

			match_pthrottling = &(match_pr->throttling);
			match_pdomain = &(match_pthrottling->domain_info);
			if (match_pdomain->domain != pdomain->domain)
				continue;

			/* Here i and j are in the same domain.
			 * If two TSD packages have the same domain, they
			 * should have the same num_porcessors and
			 * coordination type. Otherwise it will be regarded
			 * as illegal.
			 */
			if (match_pdomain->num_processors != count_target) {
				retval = -EINVAL;
				goto err_ret;
			}

			if (pdomain->coord_type != match_pdomain->coord_type) {
				retval = -EINVAL;
				goto err_ret;
			}

			cpu_set(j, covered_cpus);
			cpu_set(j, pthrottling->shared_cpu_map);
			count++;
		}
		for_each_possible_cpu(j) {
			if (i == j)
				continue;

			match_pr = processors[j];
			if (!match_pr)
				continue;

			match_pthrottling = &(match_pr->throttling);
			match_pdomain = &(match_pthrottling->domain_info);
			if (match_pdomain->domain != pdomain->domain)
				continue;

			/*
			 * If some CPUS have the same domain, they
			 * will have the same shared_cpu_map.
			 */
			match_pthrottling->shared_cpu_map =
				pthrottling->shared_cpu_map;
		}
	}

err_ret:
	for_each_possible_cpu(i) {
		pr = processors[i];
		if (!pr)
			continue;

		/*
		 * Assume no coordination on any error parsing domain info.
		 * The coordination type will be forced as SW_ALL.
		 */
		if (retval) {
			pthrottling = &(pr->throttling);
			cpus_clear(pthrottling->shared_cpu_map);
			cpu_set(i, pthrottling->shared_cpu_map);
			pthrottling->shared_type = DOMAIN_COORD_TYPE_SW_ALL;
		}
	}

	return retval;
}

/*
 * Update the T-state coordination after the _TSD
 * data for all cpus is obtained.
 */
void acpi_processor_throttling_init(void)
{
	if (acpi_processor_update_tsd_coord())
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"Assume no T-state coordination\n"));

	return;
}

static int acpi_processor_throttling_notifier(unsigned long event, void *data)
{
	struct throttling_tstate *p_tstate = data;
	struct acpi_processor *pr;
	unsigned int cpu ;
	int target_state;
	struct acpi_processor_limit *p_limit;
	struct acpi_processor_throttling *p_throttling;

	cpu = p_tstate->cpu;
	pr = processors[cpu];
	if (!pr) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Invalid pr pointer\n"));
		return 0;
	}
	if (!pr->flags.throttling) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Throttling control is "
				"unsupported on CPU %d\n", cpu));
		return 0;
	}
	target_state = p_tstate->target_state;
	p_throttling = &(pr->throttling);
	switch (event) {
	case THROTTLING_PRECHANGE:
		/*
		 * Prechange event is used to choose one proper t-state,
		 * which meets the limits of thermal, user and _TPC.
		 */
		p_limit = &pr->limit;
		if (p_limit->thermal.tx > target_state)
			target_state = p_limit->thermal.tx;
		if (p_limit->user.tx > target_state)
			target_state = p_limit->user.tx;
		if (pr->throttling_platform_limit > target_state)
			target_state = pr->throttling_platform_limit;
		if (target_state >= p_throttling->state_count) {
			printk(KERN_WARNING
				"Exceed the limit of T-state \n");
			target_state = p_throttling->state_count - 1;
		}
		p_tstate->target_state = target_state;
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "PreChange Event:"
				"target T-state of CPU %d is T%d\n",
				cpu, target_state));
		break;
	case THROTTLING_POSTCHANGE:
		/*
		 * Postchange event is only used to update the
		 * T-state flag of acpi_processor_throttling.
		 */
		p_throttling->state = target_state;
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "PostChange Event:"
				"CPU %d is switched to T%d\n",
				cpu, target_state));
		break;
	default:
		printk(KERN_WARNING
			"Unsupported Throttling notifier event\n");
		break;
	}

	return 0;
}

/*
 * _TPC - Throttling Present Capabilities
 */
static int acpi_processor_get_platform_limit(struct acpi_processor *pr)
{
	acpi_status status = 0;
	unsigned long tpc = 0;

	if (!pr)
		return -EINVAL;
	status = acpi_evaluate_integer(pr->handle, "_TPC", NULL, &tpc);
	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			ACPI_EXCEPTION((AE_INFO, status, "Evaluating _TPC"));
		}
		return -ENODEV;
	}
	pr->throttling_platform_limit = (int)tpc;
	return 0;
}

int acpi_processor_tstate_has_changed(struct acpi_processor *pr)
{
	int result = 0;
	int throttling_limit;
	int current_state;
	struct acpi_processor_limit *limit;
	int target_state;

	result = acpi_processor_get_platform_limit(pr);
	if (result) {
		/* Throttling Limit is unsupported */
		return result;
	}

	throttling_limit = pr->throttling_platform_limit;
	if (throttling_limit >= pr->throttling.state_count) {
		/* Uncorrect Throttling Limit */
		return -EINVAL;
	}

	current_state = pr->throttling.state;
	if (current_state > throttling_limit) {
		/*
		 * The current state can meet the requirement of
		 * _TPC limit. But it is reasonable that OSPM changes
		 * t-states from high to low for better performance.
		 * Of course the limit condition of thermal
		 * and user should be considered.
		 */
		limit = &pr->limit;
		target_state = throttling_limit;
		if (limit->thermal.tx > target_state)
			target_state = limit->thermal.tx;
		if (limit->user.tx > target_state)
			target_state = limit->user.tx;
	} else if (current_state == throttling_limit) {
		/*
		 * Unnecessary to change the throttling state
		 */
		return 0;
	} else {
		/*
		 * If the current state is lower than the limit of _TPC, it
		 * will be forced to switch to the throttling state defined
		 * by throttling_platfor_limit.
		 * Because the previous state meets with the limit condition
		 * of thermal and user, it is unnecessary to check it again.
		 */
		target_state = throttling_limit;
	}
	return acpi_processor_set_throttling(pr, target_state);
}

/*
 * _PTC - Processor Throttling Control (and status) register location
 */
static int acpi_processor_get_throttling_control(struct acpi_processor *pr)
{
	int result = 0;
	acpi_status status = 0;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *ptc = NULL;
	union acpi_object obj = { 0 };
	struct acpi_processor_throttling *throttling;

	status = acpi_evaluate_object(pr->handle, "_PTC", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			ACPI_EXCEPTION((AE_INFO, status, "Evaluating _PTC"));
		}
		return -ENODEV;
	}

	ptc = (union acpi_object *)buffer.pointer;
	if (!ptc || (ptc->type != ACPI_TYPE_PACKAGE)
	    || (ptc->package.count != 2)) {
		printk(KERN_ERR PREFIX "Invalid _PTC data\n");
		result = -EFAULT;
		goto end;
	}

	/*
	 * control_register
	 */

	obj = ptc->package.elements[0];

	if ((obj.type != ACPI_TYPE_BUFFER)
	    || (obj.buffer.length < sizeof(struct acpi_ptc_register))
	    || (obj.buffer.pointer == NULL)) {
		printk(KERN_ERR PREFIX
		       "Invalid _PTC data (control_register)\n");
		result = -EFAULT;
		goto end;
	}
	memcpy(&pr->throttling.control_register, obj.buffer.pointer,
	       sizeof(struct acpi_ptc_register));

	/*
	 * status_register
	 */

	obj = ptc->package.elements[1];

	if ((obj.type != ACPI_TYPE_BUFFER)
	    || (obj.buffer.length < sizeof(struct acpi_ptc_register))
	    || (obj.buffer.pointer == NULL)) {
		printk(KERN_ERR PREFIX "Invalid _PTC data (status_register)\n");
		result = -EFAULT;
		goto end;
	}

	memcpy(&pr->throttling.status_register, obj.buffer.pointer,
	       sizeof(struct acpi_ptc_register));

	throttling = &pr->throttling;

	if ((throttling->control_register.bit_width +
		throttling->control_register.bit_offset) > 32) {
		printk(KERN_ERR PREFIX "Invalid _PTC control register\n");
		result = -EFAULT;
		goto end;
	}

	if ((throttling->status_register.bit_width +
		throttling->status_register.bit_offset) > 32) {
		printk(KERN_ERR PREFIX "Invalid _PTC status register\n");
		result = -EFAULT;
		goto end;
	}

      end:
	kfree(buffer.pointer);

	return result;
}

/*
 * _TSS - Throttling Supported States
 */
static int acpi_processor_get_throttling_states(struct acpi_processor *pr)
{
	int result = 0;
	acpi_status status = AE_OK;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_buffer format = { sizeof("NNNNN"), "NNNNN" };
	struct acpi_buffer state = { 0, NULL };
	union acpi_object *tss = NULL;
	int i;

	status = acpi_evaluate_object(pr->handle, "_TSS", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			ACPI_EXCEPTION((AE_INFO, status, "Evaluating _TSS"));
		}
		return -ENODEV;
	}

	tss = buffer.pointer;
	if (!tss || (tss->type != ACPI_TYPE_PACKAGE)) {
		printk(KERN_ERR PREFIX "Invalid _TSS data\n");
		result = -EFAULT;
		goto end;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found %d throttling states\n",
			  tss->package.count));

	pr->throttling.state_count = tss->package.count;
	pr->throttling.states_tss =
	    kmalloc(sizeof(struct acpi_processor_tx_tss) * tss->package.count,
		    GFP_KERNEL);
	if (!pr->throttling.states_tss) {
		result = -ENOMEM;
		goto end;
	}

	for (i = 0; i < pr->throttling.state_count; i++) {

		struct acpi_processor_tx_tss *tx =
		    (struct acpi_processor_tx_tss *)&(pr->throttling.
						      states_tss[i]);

		state.length = sizeof(struct acpi_processor_tx_tss);
		state.pointer = tx;

		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Extracting state %d\n", i));

		status = acpi_extract_package(&(tss->package.elements[i]),
					      &format, &state);
		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION((AE_INFO, status, "Invalid _TSS data"));
			result = -EFAULT;
			kfree(pr->throttling.states_tss);
			goto end;
		}

		if (!tx->freqpercentage) {
			printk(KERN_ERR PREFIX
			       "Invalid _TSS data: freq is zero\n");
			result = -EFAULT;
			kfree(pr->throttling.states_tss);
			goto end;
		}
	}

      end:
	kfree(buffer.pointer);

	return result;
}

/*
 * _TSD - T-State Dependencies
 */
static int acpi_processor_get_tsd(struct acpi_processor *pr)
{
	int result = 0;
	acpi_status status = AE_OK;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_buffer format = { sizeof("NNNNN"), "NNNNN" };
	struct acpi_buffer state = { 0, NULL };
	union acpi_object *tsd = NULL;
	struct acpi_tsd_package *pdomain;
	struct acpi_processor_throttling *pthrottling;

	pthrottling = &pr->throttling;
	pthrottling->tsd_valid_flag = 0;

	status = acpi_evaluate_object(pr->handle, "_TSD", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			ACPI_EXCEPTION((AE_INFO, status, "Evaluating _TSD"));
		}
		return -ENODEV;
	}

	tsd = buffer.pointer;
	if (!tsd || (tsd->type != ACPI_TYPE_PACKAGE)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Invalid _TSD data\n"));
		result = -EFAULT;
		goto end;
	}

	if (tsd->package.count != 1) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Invalid _TSD data\n"));
		result = -EFAULT;
		goto end;
	}

	pdomain = &(pr->throttling.domain_info);

	state.length = sizeof(struct acpi_tsd_package);
	state.pointer = pdomain;

	status = acpi_extract_package(&(tsd->package.elements[0]),
				      &format, &state);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Invalid _TSD data\n"));
		result = -EFAULT;
		goto end;
	}

	if (pdomain->num_entries != ACPI_TSD_REV0_ENTRIES) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Unknown _TSD:num_entries\n"));
		result = -EFAULT;
		goto end;
	}

	if (pdomain->revision != ACPI_TSD_REV0_REVISION) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Unknown _TSD:revision\n"));
		result = -EFAULT;
		goto end;
	}

	pthrottling = &pr->throttling;
	pthrottling->tsd_valid_flag = 1;
	pthrottling->shared_type = pdomain->coord_type;
	cpu_set(pr->id, pthrottling->shared_cpu_map);
	/*
	 * If the coordination type is not defined in ACPI spec,
	 * the tsd_valid_flag will be clear and coordination type
	 * will be forecd as DOMAIN_COORD_TYPE_SW_ALL.
	 */
	if (pdomain->coord_type != DOMAIN_COORD_TYPE_SW_ALL &&
		pdomain->coord_type != DOMAIN_COORD_TYPE_SW_ANY &&
		pdomain->coord_type != DOMAIN_COORD_TYPE_HW_ALL) {
		pthrottling->tsd_valid_flag = 0;
		pthrottling->shared_type = DOMAIN_COORD_TYPE_SW_ALL;
	}

      end:
	kfree(buffer.pointer);
	return result;
}

/* --------------------------------------------------------------------------
                              Throttling Control
   -------------------------------------------------------------------------- */
static int acpi_processor_get_throttling_fadt(struct acpi_processor *pr)
{
	int state = 0;
	u32 value = 0;
	u32 duty_mask = 0;
	u32 duty_value = 0;

	if (!pr)
		return -EINVAL;

	if (!pr->flags.throttling)
		return -ENODEV;

	pr->throttling.state = 0;

	duty_mask = pr->throttling.state_count - 1;

	duty_mask <<= pr->throttling.duty_offset;

	local_irq_disable();

	value = inl(pr->throttling.address);

	/*
	 * Compute the current throttling state when throttling is enabled
	 * (bit 4 is on).
	 */
	if (value & 0x10) {
		duty_value = value & duty_mask;
		duty_value >>= pr->throttling.duty_offset;

		if (duty_value)
			state = pr->throttling.state_count - duty_value;
	}

	pr->throttling.state = state;

	local_irq_enable();

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "Throttling state is T%d (%d%% throttling applied)\n",
			  state, pr->throttling.states[state].performance));

	return 0;
}

#ifdef CONFIG_X86
static int acpi_throttling_rdmsr(struct acpi_processor *pr,
					acpi_integer * value)
{
	struct cpuinfo_x86 *c;
	u64 msr_high, msr_low;
	unsigned int cpu;
	u64 msr = 0;
	int ret = -1;

	cpu = pr->id;
	c = &cpu_data[cpu];

	if ((c->x86_vendor != X86_VENDOR_INTEL) ||
		!cpu_has(c, X86_FEATURE_ACPI)) {
		printk(KERN_ERR PREFIX
			"HARDWARE addr space,NOT supported yet\n");
	} else {
		msr_low = 0;
		msr_high = 0;
		rdmsr_safe(MSR_IA32_THERM_CONTROL,
			(u32 *)&msr_low , (u32 *) &msr_high);
		msr = (msr_high << 32) | msr_low;
		*value = (acpi_integer) msr;
		ret = 0;
	}
	return ret;
}

static int acpi_throttling_wrmsr(struct acpi_processor *pr, acpi_integer value)
{
	struct cpuinfo_x86 *c;
	unsigned int cpu;
	int ret = -1;
	u64 msr;

	cpu = pr->id;
	c = &cpu_data[cpu];

	if ((c->x86_vendor != X86_VENDOR_INTEL) ||
		!cpu_has(c, X86_FEATURE_ACPI)) {
		printk(KERN_ERR PREFIX
			"HARDWARE addr space,NOT supported yet\n");
	} else {
		msr = value;
		wrmsr_safe(MSR_IA32_THERM_CONTROL,
			   (u32)msr, (u32)(msr >> 32));
		ret = 0;
	}
	return ret;
}
#else
static int acpi_throttling_rdmsr(struct acpi_processor *pr,
				acpi_integer * value)
{
	printk(KERN_ERR PREFIX
		"HARDWARE addr space,NOT supported yet\n");
	return -1;
}

static int acpi_throttling_wrmsr(struct acpi_processor *pr, acpi_integer value)
{
	printk(KERN_ERR PREFIX
		"HARDWARE addr space,NOT supported yet\n");
	return -1;
}
#endif

static int acpi_read_throttling_status(struct acpi_processor *pr,
					acpi_integer *value)
{
	u32 bit_width, bit_offset;
	u64 ptc_value;
	u64 ptc_mask;
	struct acpi_processor_throttling *throttling;
	int ret = -1;

	throttling = &pr->throttling;
	switch (throttling->status_register.space_id) {
	case ACPI_ADR_SPACE_SYSTEM_IO:
		ptc_value = 0;
		bit_width = throttling->status_register.bit_width;
		bit_offset = throttling->status_register.bit_offset;

		acpi_os_read_port((acpi_io_address) throttling->status_register.
				  address, (u32 *) &ptc_value,
				  (u32) (bit_width + bit_offset));
		ptc_mask = (1 << bit_width) - 1;
		*value = (acpi_integer) ((ptc_value >> bit_offset) & ptc_mask);
		ret = 0;
		break;
	case ACPI_ADR_SPACE_FIXED_HARDWARE:
		ret = acpi_throttling_rdmsr(pr, value);
		break;
	default:
		printk(KERN_ERR PREFIX "Unknown addr space %d\n",
		       (u32) (throttling->status_register.space_id));
	}
	return ret;
}

static int acpi_write_throttling_state(struct acpi_processor *pr,
				acpi_integer value)
{
	u32 bit_width, bit_offset;
	u64 ptc_value;
	u64 ptc_mask;
	struct acpi_processor_throttling *throttling;
	int ret = -1;

	throttling = &pr->throttling;
	switch (throttling->control_register.space_id) {
	case ACPI_ADR_SPACE_SYSTEM_IO:
		bit_width = throttling->control_register.bit_width;
		bit_offset = throttling->control_register.bit_offset;
		ptc_mask = (1 << bit_width) - 1;
		ptc_value = value & ptc_mask;

		acpi_os_write_port((acpi_io_address) throttling->
					control_register.address,
					(u32) (ptc_value << bit_offset),
					(u32) (bit_width + bit_offset));
		ret = 0;
		break;
	case ACPI_ADR_SPACE_FIXED_HARDWARE:
		ret = acpi_throttling_wrmsr(pr, value);
		break;
	default:
		printk(KERN_ERR PREFIX "Unknown addr space %d\n",
		       (u32) (throttling->control_register.space_id));
	}
	return ret;
}

static int acpi_get_throttling_state(struct acpi_processor *pr,
				acpi_integer value)
{
	int i;

	for (i = 0; i < pr->throttling.state_count; i++) {
		struct acpi_processor_tx_tss *tx =
		    (struct acpi_processor_tx_tss *)&(pr->throttling.
						      states_tss[i]);
		if (tx->control == value)
			break;
	}
	if (i > pr->throttling.state_count)
		i = -1;
	return i;
}

static int acpi_get_throttling_value(struct acpi_processor *pr,
			int state, acpi_integer *value)
{
	int ret = -1;

	if (state >= 0 && state <= pr->throttling.state_count) {
		struct acpi_processor_tx_tss *tx =
		    (struct acpi_processor_tx_tss *)&(pr->throttling.
						      states_tss[state]);
		*value = tx->control;
		ret = 0;
	}
	return ret;
}

static int acpi_processor_get_throttling_ptc(struct acpi_processor *pr)
{
	int state = 0;
	int ret;
	acpi_integer value;

	if (!pr)
		return -EINVAL;

	if (!pr->flags.throttling)
		return -ENODEV;

	pr->throttling.state = 0;

	value = 0;
	ret = acpi_read_throttling_status(pr, &value);
	if (ret >= 0) {
		state = acpi_get_throttling_state(pr, value);
		pr->throttling.state = state;
	}

	return 0;
}

static int acpi_processor_get_throttling(struct acpi_processor *pr)
{
	cpumask_t saved_mask;
	int ret;

	if (!pr)
		return -EINVAL;

	if (!pr->flags.throttling)
		return -ENODEV;
	/*
	 * Migrate task to the cpu pointed by pr.
	 */
	saved_mask = current->cpus_allowed;
	set_cpus_allowed(current, cpumask_of_cpu(pr->id));
	ret = pr->throttling.acpi_processor_get_throttling(pr);
	/* restore the previous state */
	set_cpus_allowed(current, saved_mask);

	return ret;
}

static int acpi_processor_get_fadt_info(struct acpi_processor *pr)
{
	int i, step;

	if (!pr->throttling.address) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No throttling register\n"));
		return -EINVAL;
	} else if (!pr->throttling.duty_width) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No throttling states\n"));
		return -EINVAL;
	}
	/* TBD: Support duty_cycle values that span bit 4. */
	else if ((pr->throttling.duty_offset + pr->throttling.duty_width) > 4) {
		printk(KERN_WARNING PREFIX "duty_cycle spans bit 4\n");
		return -EINVAL;
	}

	pr->throttling.state_count = 1 << acpi_fadt.duty_width;

	/*
	 * Compute state values. Note that throttling displays a linear power
	 * performance relationship (at 50% performance the CPU will consume
	 * 50% power).  Values are in 1/10th of a percent to preserve accuracy.
	 */

	step = (1000 / pr->throttling.state_count);

	for (i = 0; i < pr->throttling.state_count; i++) {
		pr->throttling.states[i].performance = 1000 - step * i;
		pr->throttling.states[i].power = 1000 - step * i;
	}
	return 0;
}

static int acpi_processor_set_throttling_fadt(struct acpi_processor *pr,
					      int state)
{
	u32 value = 0;
	u32 duty_mask = 0;
	u32 duty_value = 0;

	if (!pr)
		return -EINVAL;

	if ((state < 0) || (state > (pr->throttling.state_count - 1)))
		return -EINVAL;

	if (!pr->flags.throttling)
		return -ENODEV;

	if (state == pr->throttling.state)
		return 0;

	if (state < pr->throttling_platform_limit)
		return -EPERM;
	/*
	 * Calculate the duty_value and duty_mask.
	 */
	if (state) {
		duty_value = pr->throttling.state_count - state;

		duty_value <<= pr->throttling.duty_offset;

		/* Used to clear all duty_value bits */
		duty_mask = pr->throttling.state_count - 1;

		duty_mask <<= acpi_fadt.duty_offset;
		duty_mask = ~duty_mask;
	}

	local_irq_disable();

	/*
	 * Disable throttling by writing a 0 to bit 4.  Note that we must
	 * turn it off before you can change the duty_value.
	 */
	value = inl(pr->throttling.address);
	if (value & 0x10) {
		value &= 0xFFFFFFEF;
		outl(value, pr->throttling.address);
	}

	/*
	 * Write the new duty_value and then enable throttling.  Note
	 * that a state value of 0 leaves throttling disabled.
	 */
	if (state) {
		value &= duty_mask;
		value |= duty_value;
		outl(value, pr->throttling.address);

		value |= 0x00000010;
		outl(value, pr->throttling.address);
	}

	pr->throttling.state = state;

	local_irq_enable();

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "Throttling state set to T%d (%d%%)\n", state,
			  (pr->throttling.states[state].performance ? pr->
			   throttling.states[state].performance / 10 : 0)));

	return 0;
}

static int acpi_processor_set_throttling_ptc(struct acpi_processor *pr,
					     int state)
{
	int ret;
	acpi_integer value;

	if (!pr)
		return -EINVAL;

	if ((state < 0) || (state > (pr->throttling.state_count - 1)))
		return -EINVAL;

	if (!pr->flags.throttling)
		return -ENODEV;

	if (state == pr->throttling.state)
		return 0;

	if (state < pr->throttling_platform_limit)
		return -EPERM;

	value = 0;
	ret = acpi_get_throttling_value(pr, state, &value);
	if (ret >= 0) {
		acpi_write_throttling_state(pr, value);
		pr->throttling.state = state;
	}

	return 0;
}

int acpi_processor_set_throttling(struct acpi_processor *pr, int state)
{
	cpumask_t saved_mask;
	int ret = 0;
	unsigned int i;
	struct acpi_processor *match_pr;
	struct acpi_processor_throttling *p_throttling;
	struct throttling_tstate t_state;
	cpumask_t online_throttling_cpus;

	if (!pr)
		return -EINVAL;

	if (!pr->flags.throttling)
		return -ENODEV;

	if ((state < 0) || (state > (pr->throttling.state_count - 1)))
		return -EINVAL;

	saved_mask = current->cpus_allowed;
	t_state.target_state = state;
	p_throttling = &(pr->throttling);
	cpus_and(online_throttling_cpus, cpu_online_map,
			p_throttling->shared_cpu_map);
	/*
	 * The throttling notifier will be called for every
	 * affected cpu in order to get one proper T-state.
	 * The notifier event is THROTTLING_PRECHANGE.
	 */
	for_each_cpu_mask(i, online_throttling_cpus) {
		t_state.cpu = i;
		acpi_processor_throttling_notifier(THROTTLING_PRECHANGE,
							&t_state);
	}
	/*
	 * The function of acpi_processor_set_throttling will be called
	 * to switch T-state. If the coordination type is SW_ALL or HW_ALL,
	 * it is necessary to call it for every affected cpu. Otherwise
	 * it can be called only for the cpu pointed by pr.
	 */
	if (p_throttling->shared_type == DOMAIN_COORD_TYPE_SW_ANY) {
		set_cpus_allowed(current, cpumask_of_cpu(pr->id));
		ret = p_throttling->acpi_processor_set_throttling(pr,
						t_state.target_state);
	} else {
		/*
		 * When the T-state coordination is SW_ALL or HW_ALL,
		 * it is necessary to set T-state for every affected
		 * cpus.
		 */
		for_each_cpu_mask(i, online_throttling_cpus) {
			match_pr = processors[i];
			/*
			 * If the pointer is invalid, we will report the
			 * error message and continue.
			 */
			if (!match_pr) {
				ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					"Invalid Pointer for CPU %d\n", i));
				continue;
			}
			/*
			 * If the throttling control is unsupported on CPU i,
			 * we will report the error message and continue.
			 */
			if (!match_pr->flags.throttling) {
				ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					"Throttling Controll is unsupported "
					"on CPU %d\n", i));
				continue;
			}
			t_state.cpu = i;
			set_cpus_allowed(current, cpumask_of_cpu(i));
			ret = match_pr->throttling.
				acpi_processor_set_throttling(
				match_pr, t_state.target_state);
		}
	}
	/*
	 * After the set_throttling is called, the
	 * throttling notifier is called for every
	 * affected cpu to update the T-states.
	 * The notifier event is THROTTLING_POSTCHANGE
	 */
	for_each_cpu_mask(i, online_throttling_cpus) {
		t_state.cpu = i;
		acpi_processor_throttling_notifier(THROTTLING_POSTCHANGE,
							&t_state);
	}
	/* restore the previous state */
	set_cpus_allowed(current, saved_mask);
	return ret;
}

int acpi_processor_get_throttling_info(struct acpi_processor *pr)
{
	int result = 0;
	struct acpi_processor_throttling *pthrottling;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "pblk_address[0x%08x] duty_offset[%d] duty_width[%d]\n",
			  pr->throttling.address,
			  pr->throttling.duty_offset,
			  pr->throttling.duty_width));

	if (!pr)
		return -EINVAL;

	/*
	 * Evaluate _PTC, _TSS and _TPC
	 * They must all be present or none of them can be used.
	 */
	if (acpi_processor_get_throttling_control(pr) ||
		acpi_processor_get_throttling_states(pr) ||
		acpi_processor_get_platform_limit(pr))
	{
		pr->throttling.acpi_processor_get_throttling =
		    &acpi_processor_get_throttling_fadt;
		pr->throttling.acpi_processor_set_throttling =
		    &acpi_processor_set_throttling_fadt;
		if (acpi_processor_get_fadt_info(pr))
			return 0;
	} else {
		pr->throttling.acpi_processor_get_throttling =
		    &acpi_processor_get_throttling_ptc;
		pr->throttling.acpi_processor_set_throttling =
		    &acpi_processor_set_throttling_ptc;
	}

	/*
	 * If TSD package for one CPU can't be parsed successfully, it means
	 * that this CPU will have no coordination with other CPUs.
	 */
	if (acpi_processor_get_tsd(pr)) {
		pthrottling = &pr->throttling;
		pthrottling->tsd_valid_flag = 0;
		cpu_set(pr->id, pthrottling->shared_cpu_map);
		pthrottling->shared_type = DOMAIN_COORD_TYPE_SW_ALL;
	}

	/*
	 * PIIX4 Errata: We don't support throttling on the original PIIX4.
	 * This shouldn't be an issue as few (if any) mobile systems ever
	 * used this part.
	 */
	if (errata.piix4.throttle) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Throttling not supported on PIIX4 A- or B-step\n"));
		return 0;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found %d throttling states\n",
			  pr->throttling.state_count));

	pr->flags.throttling = 1;

	/*
	 * Disable throttling (if enabled).  We'll let subsequent policy (e.g.
	 * thermal) decide to lower performance if it so chooses, but for now
	 * we'll crank up the speed.
	 */

	result = acpi_processor_get_throttling(pr);
	if (result)
		goto end;

	if (pr->throttling.state) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Disabling throttling (was T%d)\n",
				  pr->throttling.state));
		result = acpi_processor_set_throttling(pr, 0);
		if (result)
			goto end;
	}

      end:
	if (result)
		pr->flags.throttling = 0;

	return result;
}

/* proc interface */

static int acpi_processor_throttling_seq_show(struct seq_file *seq,
					      void *offset)
{
	struct acpi_processor *pr = seq->private;
	int i = 0;
	int result = 0;

	if (!pr)
		goto end;

	if (!(pr->throttling.state_count > 0)) {
		seq_puts(seq, "<not supported>\n");
		goto end;
	}

	result = acpi_processor_get_throttling(pr);

	if (result) {
		seq_puts(seq,
			 "Could not determine current throttling state.\n");
		goto end;
	}

	seq_printf(seq, "state count:             %d\n"
		   "active state:            T%d\n"
		   "state available: T%d to T%d\n",
		   pr->throttling.state_count, pr->throttling.state,
		   pr->throttling_platform_limit,
		   pr->throttling.state_count - 1);

	seq_puts(seq, "states:\n");
	if (pr->throttling.acpi_processor_get_throttling ==
			acpi_processor_get_throttling_fadt) {
		for (i = 0; i < pr->throttling.state_count; i++)
			seq_printf(seq, "   %cT%d:                  %02d%%\n",
				   (i == pr->throttling.state ? '*' : ' '), i,
				   (pr->throttling.states[i].performance ? pr->
				    throttling.states[i].performance / 10 : 0));
	} else {
		for (i = 0; i < pr->throttling.state_count; i++)
			seq_printf(seq, "   %cT%d:                  %02d%%\n",
				   (i == pr->throttling.state ? '*' : ' '), i,
				   (int)pr->throttling.states_tss[i].
				   freqpercentage);
	}

      end:
	return 0;
}

static int acpi_processor_throttling_open_fs(struct inode *inode,
					     struct file *file)
{
	return single_open(file, acpi_processor_throttling_seq_show,
			   PDE(inode)->data);
}

static ssize_t acpi_processor_write_throttling(struct file *file,
					       const char __user * buffer,
					       size_t count, loff_t * data)
{
	int result = 0;
	struct seq_file *m = file->private_data;
	struct acpi_processor *pr = m->private;
	char state_string[5] = "";
	char *charp = NULL;
	size_t state_val = 0;
	char tmpbuf[5] = "";

	if (!pr || (count > sizeof(state_string) - 1))
		return -EINVAL;

	if (copy_from_user(state_string, buffer, count))
		return -EFAULT;

	state_string[count] = '\0';
	if ((count > 0) && (state_string[count-1] == '\n'))
		state_string[count-1] = '\0';

	charp = state_string;
	if ((state_string[0] == 't') || (state_string[0] == 'T'))
		charp++;

	state_val = simple_strtoul(charp, NULL, 0);
	if (state_val >= pr->throttling.state_count)
		return -EINVAL;

	snprintf(tmpbuf, 5, "%zu", state_val);

	if (strcmp(tmpbuf, charp) != 0)
		return -EINVAL;

	result = acpi_processor_set_throttling(pr, state_val);
	if (result)
		return result;

	return count;
}

struct file_operations acpi_processor_throttling_fops = {
	.owner = THIS_MODULE,
	.open = acpi_processor_throttling_open_fs,
	.read = seq_read,
	.write = acpi_processor_write_throttling,
	.llseek = seq_lseek,
	.release = single_release,
};
