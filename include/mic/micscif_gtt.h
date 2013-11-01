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

#ifndef MICSCIF_GTT_H
#define MICSCIF_GTT_H

/* Call this from driver initialization */
int micscif_init_gtt(struct micscif_dev *scif_dev);

/*
 * Finds a set of open GTT entries to map the local addr, out_offset is
 * the output parameter (which is the offset into the BAR for this local addr
 * after we're done)
 */
int micscif_map_gtt(phys_addr_t *out_offset, phys_addr_t local_addr, int len,
		    struct micscif_dev *scif_dev);

int micscif_unmap_gtt_offset(phys_addr_t offset, int len,
			     struct micscif_dev *scif_dev);

#ifndef _MIC_SCIF_
int micscif_map_gtt_dma(phys_addr_t *out_offset, phys_addr_t local_addr,
	int len, struct micscif_dev *scif_dev,
	int ch_num, struct mic_copy_work *work, struct endpt *ep);
int micscif_unmap_gtt_dma(struct mic_copy_work *work);
#endif

#endif /* MICSCIF_GTT_H */
