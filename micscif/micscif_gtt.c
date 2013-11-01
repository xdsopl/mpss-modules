/*
 * Copyright 2010-2013 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Disclaimer: The codes contained in these modules may be specific to
 * the Intel Software Development Platform codenamed Knights Ferry,
 * and the Intel product codenamed Knights Corner, and are not backward
 * compatible with other Intel products. Additionally, Intel will NOT
 * support the codes or instruction set in future products.
 *
 * Intel offers no warranty of any kind regarding the code. This code is
 * licensed on an "AS IS" basis and Intel is not obligated to provide
 * any support, assistance, installation, training, or other services
 * of any kind. Intel is also not obligated to provide any updates,
 * enhancements or extensions. Intel specifically disclaims any warranty
 * of merchantability, non-infringement, fitness for any particular
 * purpose, and any other warranty.
 *
 * Further, Intel disclaims all liability of any kind, including but
 * not limited to liability for infringement of any proprietary rights,
 * relating to the use of the code, even if Intel is notified of the
 * possibility of such liability. Except as expressly stated in an Intel
 * license agreement provided with this code and agreed upon with Intel,
 * no license, express or implied, by estoppel or otherwise, to any
 * intellectual property rights is granted herein.
 */

#include <mic/micscif.h>
#ifndef _MIC_SCIF_
#include "mic_common.h"
#endif
#include "mic/micscif_map.h"

#if defined(CONFIG_ML1OM)
static struct mutex gtt_lock;
static struct va_gen_addr gtt_offset_gen;
#endif

/*
 * Fire gtt entry flush - this forces hardware
 * to reload GTT table
 */
static inline void flush_gtt(volatile void *mm_sbox)
{
	writel(1, (uint8_t *)mm_sbox + SBOX_SBQ_FLUSH);
	writel(1, (uint8_t *)mm_sbox + SBOX_TLB_FLUSH);
}

#ifndef _MIC_SCIF_
/*
 * We don't need a lock to protect this code because
 * i)there can be only single call to this fn for a specific channel and
 * ii) different channels touch different entries
 */
int micscif_map_gtt_dma(phys_addr_t *out_offset,
	phys_addr_t local_addr, int len, struct micscif_dev *scif_dev,
	int ch_num, struct mic_copy_work *work, struct endpt *ep)
{
	mic_ctx_t *mic_ctx = get_per_dev_ctx(scif_dev->sd_node - 1);
	int err = 0;

#if defined(CONFIG_ML1OM)
	if (FAMILY_ABR == mic_ctx->bi_family) {
		uint32_t local_addr_page_offset = local_addr & (PAGE_SIZE - 1);
		phys_addr_t aligned_local_addr = align_low(local_addr, PAGE_SIZE);
		int aligned_len = ALIGN(len + local_addr_page_offset, PAGE_SIZE);
		struct nodemsg msg;

		work->gttmap_state = OP_IN_PROGRESS;
		msg.uop = SCIF_DMA_GTT_MAP;
		msg.payload[0] = (uint64_t)work;
		msg.payload[1] = aligned_local_addr;
		msg.payload[2] = aligned_len;

		if ((err = micscif_nodeqp_send(scif_dev, &msg, NULL))) {
			return err;
		}
retry:
		err = wait_event_timeout(work->gttmapwq, 
			work->gttmap_state != OP_IN_PROGRESS, NODE_ALIVE_TIMEOUT);
		if (!err && scifdev_alive(ep))
			goto retry;

		if (!err) {
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
			return -ENODEV;
		}
		if (OP_FAILED == work->gttmap_state) {
			printk(KERN_ERR "%s %d err %d\n", __func__, __LINE__, err);
			return -ENOMEM;
		}
		if (err > 0)
			err = 0;

		*out_offset = work->gtt_offset | local_addr_page_offset;
	}
#elif defined(CONFIG_MK1OM)
	if (FAMILY_KNC == mic_ctx->bi_family) {
		/*
		 * TODO: Remove "ifdef CONFIG_MK1OM" above
		 * once KNC Power On branch is created to allow host driver
		 * to work for both KNF/KNC without re-compilation. We cannot
		 * remove the ifdef's immediately in master since Alpha7
		 * releases need to ship without CONFIG_MK1OM code
		 * determined at compile time.
		 */
		/*
		 * For KNC, there is no GTT. Everything is an offset relative to
		 * APR_PHY_BASE (most likely 0).
		 */
		*out_offset = local_addr;
	}
#endif
	return err;
}

int micscif_unmap_gtt_dma(struct mic_copy_work *work)
{
	int err = 0;
#if defined(CONFIG_ML1OM)
	mic_ctx_t *mic_ctx = get_per_dev_ctx(work->remote_dev->sd_node - 1);
	if (FAMILY_ABR == mic_ctx->bi_family) {
		struct nodemsg msg;
		msg.uop = SCIF_DMA_GTT_UNMAP;
		msg.payload[0] = work->gtt_offset;
		msg.payload[1] = work->gtt_length;

		if ((err = micscif_nodeqp_send(work->remote_dev, &msg, NULL))) {
			return err;
		}
	}
#endif
	return err;
}
#endif
/*
 * Finds a set of open GTT entries to map the local addr, out_offset is
 * the output parameter (which is the offset into the BAR for this local addr
 * after we're done)
 */
int micscif_map_gtt(phys_addr_t *out_offset,
	phys_addr_t local_addr, int len, struct micscif_dev *scif_dev)
{
	int error = 0;
#ifdef CONFIG_ML1OM
	volatile uint32_t *gtt = (volatile uint32_t *)scif_dev->mm_gtt;
	uint32_t local_addr_page_offset = local_addr & (PAGE_SIZE - 1);
	int aligned_len = ALIGN(len + local_addr_page_offset, PAGE_SIZE);
	phys_addr_t aligned_local_addr = align_low(local_addr, PAGE_SIZE);
	uint64_t i = 0;
	int j = 0;

	mutex_lock(&gtt_lock);
	i = va_gen_alloc(&gtt_offset_gen, aligned_len, PAGE_SIZE);
	if (INVALID_VA_GEN_ADDRESS == i)
		error = -ENOMEM;
	if (!error) {
		i >>= PAGE_SHIFT;
		flush_gtt(scif_dev->mm_sbox);
		wmb();
		/*
		 * Output index in GTT where this mapping will begin.
		 */
		*out_offset = (i * PAGE_SIZE) | local_addr_page_offset;
		/*
		 * Slots available. Populate GTT, set refcount to 1
		 */
		for (j = i; j < i + aligned_len/PAGE_SIZE; j++) {
			gtt[j] = (aligned_local_addr >> (PAGE_SHIFT - 1)) | 1;
			aligned_local_addr += PAGE_SIZE;
			ms_info.nr_gtt_entries++;
		}
		wmb();
		flush_gtt(scif_dev->mm_sbox);
	} else
		*out_offset = 0x0;
	mutex_unlock(&gtt_lock);
#else
	/*
	 * For KNC, there is not GTT. Everything is an offset relative to
	 * APR_PHY_BASE (most likely 0). We simply return the local_addr.
	 */
	*out_offset = local_addr;
	error = 0;
#endif
	return error;
}

int micscif_unmap_gtt_offset(phys_addr_t offset, int len,
		struct micscif_dev *scif_dev)
{
	int error = 0;
#ifdef CONFIG_ML1OM
	int i = 0;
	volatile uint32_t *gtt = (volatile uint32_t *)scif_dev->mm_gtt;
	uint32_t page_offset = offset & (PAGE_SIZE - 1);
	int start_index = align_low(offset, PAGE_SIZE)/PAGE_SIZE;
	int end_index = start_index +
		(ALIGN(len + page_offset, PAGE_SIZE)/PAGE_SIZE);
	if (start_index > MIC_GTT_SIZE/sizeof(uint32_t)) {
		printk(KERN_ERR "%s %d start_index 0x%x end_index 0x%x"
				" offset 0x%llx len 0x%x\n", 
		__func__, __LINE__, start_index, end_index, offset, len);
		BUG_ON(start_index > MIC_GTT_SIZE/sizeof(uint32_t));
	}
	if ((end_index - start_index) > MIC_GTT_SIZE/sizeof(uint32_t)) {
		printk(KERN_ERR "%s %d start_index 0x%x end_index 0x%x"
				" offset 0x%llx len 0x%x\n", 
		__func__, __LINE__, start_index, end_index, offset, len);
		BUG_ON((end_index - start_index)
					> MIC_GTT_SIZE/sizeof(uint32_t));
	}

	mutex_lock(&gtt_lock);
	va_gen_free(&gtt_offset_gen,
		align_low(offset, PAGE_SIZE),
		ALIGN(len + page_offset, PAGE_SIZE));

	for (i = start_index; i < end_index; i++) {
		if (!(gtt[i] & 0x1)) {
			mutex_unlock(&gtt_lock);
			printk(KERN_ERR "%s %d start_index 0x%x end_index 0x%x"
				" offset 0x%llx len 0x%x gtt 0x%x\n", 
				__func__, __LINE__, start_index, 
				end_index, offset, len, gtt[i]);
			/*
			 * Unmapping without mapping?
			 */
			BUG_ON(!(gtt[i] & 0x1));
		}
		flush_gtt(scif_dev->mm_sbox);
		wmb();
		gtt[i] = 0x0;
		wmb();
		flush_gtt(scif_dev->mm_sbox);
		ms_info.nr_gtt_entries--;
	}
	mutex_unlock(&gtt_lock);
#else
	/*
	 * For KNC, there is no GTT and we didn't allocate
	 * any GTT entries. Simply return success.
	 */
	error = 0;
#endif
	return error;
}

/* Call this from driver initialization */
int micscif_init_gtt(struct micscif_dev *scif_dev)
{
#ifdef CONFIG_ML1OM
	volatile uint32_t *gtt = (volatile uint32_t *)scif_dev->mm_gtt;
	int i = 0;

	mutex_init (&gtt_lock);
	
	va_gen_init(&gtt_offset_gen, 0,
			(MIC_GTT_SIZE/sizeof(uint32_t)) * PAGE_SIZE);
	/* Clean slate for GTT's */
	va_gen_alloc(&gtt_offset_gen, PAGE_SIZE, PAGE_SIZE);
	flush_gtt(scif_dev->mm_sbox);
	wmb();
	for (i = 0; i < MIC_GTT_SIZE/sizeof(uint32_t); i++) 
		gtt[i] = 0;
	wmb();
	flush_gtt(scif_dev->mm_sbox);
	pr_debug("GTT Initialization Complete\n");
#endif
	return 0;
}
