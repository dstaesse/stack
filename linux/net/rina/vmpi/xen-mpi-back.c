/* A vmpi-impl hypervisor implementation for Xen (host side)
 *
 * Copyright 2014 Vincenzo Maffione <v.maffione@nextworks.it> Nextworks
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xen-mpi-back-common.h"

#include <linux/kthread.h>
#include <linux/if_vlan.h>
#include <linux/udp.h>

#include <net/tcp.h>

#include <xen/xen.h>
#include <xen/events.h>
#include <xen/interface/memory.h>

#include <asm/xen/hypercall.h>
#include <asm/xen/page.h>

/* Provide an option to disable split event channels at load time as
 * event channels are limited resource. Split event channels are
 * enabled by default.
 */
bool separate_tx_rx_irq = 1;
module_param(separate_tx_rx_irq, bool, 0644);

static void xenmpi_idx_release(struct vmpi_impl_info *vif, u16 pending_idx,
			       u8 status);

static void make_tx_response(struct vmpi_impl_info *vif,
			     struct xen_mpi_tx_request *txp,
			     s8       st);

static inline int tx_work_todo(struct vmpi_impl_info *vif);
static inline int rx_work_todo(struct vmpi_impl_info *vif);

static struct xen_mpi_rx_response *make_rx_response(struct vmpi_impl_info *vif,
					     u16      id,
					     s8       st,
					     u16      size,
					     u16      flags);

static inline unsigned long idx_to_pfn(struct vmpi_impl_info *vif,
				       u16 idx)
{
	return page_to_pfn(vif->mmap_pages[idx]);
}

static inline unsigned long idx_to_kaddr(struct vmpi_impl_info *vif,
					 u16 idx)
{
	return (unsigned long)pfn_to_kaddr(idx_to_pfn(vif, idx));
}

/* This is a miniumum size for the linear area to avoid lots of
 * calls to __pskb_pull_tail() as we set up checksum offsets. The
 * value 128 was chosen as it covers all IPv4 and most likely
 * IPv6 headers.
 */
#define PKT_PROT_LEN 128

static inline pending_ring_idx_t pending_index(unsigned i)
{
	return i & (MAX_PENDING_REQS-1);
}

static inline pending_ring_idx_t nr_pending_reqs(struct vmpi_impl_info *vif)
{
	return MAX_PENDING_REQS -
		vif->pending_prod + vif->pending_cons;
}

bool xenmpi_rx_ring_slots_available(struct vmpi_impl_info *vif, int needed)
{
	RING_IDX prod, cons;

	do {
		prod = vif->rx.sring->req_prod;
		cons = vif->rx.req_cons;

		if (prod - cons >= needed)
			return true;

		vif->rx.sring->req_event = prod + 1;

		/* Make sure event is visible before we check prod
		 * again.
		 */
		mb();
	} while (vif->rx.sring->req_prod != prod);

	return false;
}

struct netrx_pending_operations {
	unsigned copy_prod, copy_cons;
	unsigned meta_prod, meta_cons;
	struct gnttab_copy *copy;
	struct xenmpi_rx_meta *meta;
};

/*
 * Prepare an SKB to be transmitted to the frontend.
 *
 * This function is responsible for allocating grant operations, meta
 * structures, etc.
 *
 * It returns the number of meta structures consumed. The number of
 * ring slots used is always equal to the number of meta slots used
 * plus the number of GSO descriptors used. Currently, we use either
 * zero GSO descriptors (for non-GSO packets) or one descriptor (for
 * frontend-side LRO).
 */
static int xenmpi_gop_skb(struct vmpi_impl_info *vif,
                          struct vmpi_buffer *buf,
			  struct netrx_pending_operations *npo)
{
	struct xen_mpi_rx_request *req;
	struct xenmpi_rx_meta *meta;
	void *src_data;
        unsigned int src_offset;
	int old_meta_prod;
        unsigned long copy_len;
	struct gnttab_copy *copy_gop;

	old_meta_prod = npo->meta_prod;

	req = RING_GET_REQUEST(&vif->rx, vif->rx.req_cons++);
	meta = npo->meta + npo->meta_prod++;

        IFV(printk("%s: get rx req: [id=%d] [off=%d] [gref=%d] [len=%d]\n",
                __func__, req->id, req->offset, req->gref, req->len));

	src_data = vmpi_buffer_hdr(buf);
        src_offset = offset_in_page(src_data);
	/* Data must not cross a page boundary. */
        copy_len = PAGE_SIZE - src_offset;
        if (copy_len > buf->len)
                copy_len = buf->len;
        if (copy_len > req->len)
                copy_len = req->len;

        copy_gop = npo->copy + npo->copy_prod++;
        copy_gop->flags = GNTCOPY_dest_gref;
        copy_gop->len = copy_len;

        copy_gop->source.domid = DOMID_SELF;
        copy_gop->source.u.gmfn = virt_to_mfn(src_data);
        copy_gop->source.offset = src_offset;

        copy_gop->dest.domid = vif->domid;
        copy_gop->dest.offset = req->offset;
        copy_gop->dest.u.ref = req->gref;

	meta->id = req->id;
        meta->size = copy_len;

	return npo->meta_prod - old_meta_prod;
}

/*
 * This is a twin to xenmpi_gop_skb.  Assume that xenmpi_gop_skb was
 * used to set up the operations on the top of
 * netrx_pending_operations, which have since been done.  Check that
 * they didn't give any errors and advance over them.
 */
static int xenmpi_check_gop(struct vmpi_impl_info *vif, int nr_meta_slots,
			    struct netrx_pending_operations *npo)
{
	struct gnttab_copy     *copy_op;
	int status = XEN_NETIF_RSP_OKAY;
	int i;

	for (i = 0; i < nr_meta_slots; i++) {
		copy_op = npo->copy + npo->copy_cons++;
		if (copy_op->status != GNTST_okay) {
			printk(
				   "Bad status %d from copy to DOM%d.\n",
				   copy_op->status, vif->domid);
			status = XEN_NETIF_RSP_ERROR;
		}
	}

	return status;
}

void xenmpi_kick_thread(struct vmpi_impl_info *vif)
{
	wake_up(&vif->wq);
}

static void xenmpi_rx_action(struct vmpi_impl_info *vif)
{
	s8 status;
	struct xen_mpi_rx_response *resp;
        struct vmpi_queue rxq;
        struct vmpi_buffer *buf;
	LIST_HEAD(notify);
	int ret;
	bool need_to_notify = false;
        int meta_slots_used;
        struct vmpi_ring *ring = vif->write;

	struct netrx_pending_operations npo = {
		.copy  = vif->grant_copy_op,
		.meta  = vif->meta,
	};

        IFV(printk("%s called\n", __func__));

        vmpi_queue_init(&rxq, 0, VMPI_BUF_SIZE);

        for (;;) {
		RING_IDX max_slots_needed = 1;

		/* If the skb may not fit then bail out now */
		if (!xenmpi_rx_ring_slots_available(vif, max_slots_needed)) {
			need_to_notify = true;
			vif->rx_last_skb_slots = max_slots_needed;
			break;
		}

		vif->rx_last_skb_slots = 0;

                if (!vmpi_ring_pending(ring))
                        break;

                buf = &ring->bufs[ring->np];
                VMPI_RING_INC(ring->np);
                IFV(printk("%s: received buf, len=%d, off=%lu\n",
                        __func__, (int)buf->len, offset_in_page(buf->p)));

		meta_slots_used = xenmpi_gop_skb(vif, buf, &npo);
                BUG_ON(meta_slots_used != 1);

                vmpi_queue_push(&rxq, buf);
        }

	BUG_ON(npo.meta_prod > ARRAY_SIZE(vif->meta));

	if (!npo.copy_prod)
		goto done;

	BUG_ON(npo.copy_prod > MAX_GRANT_COPY_OPS);
	gnttab_batch_copy(vif->grant_copy_op, npo.copy_prod);

	while ((buf = vmpi_queue_pop(&rxq)) != NULL) {
                int len;

		status = xenmpi_check_gop(vif, 1, &npo);

                len = buf->len;
                buf->len = 0;
                VMPI_RING_INC(ring->nr);
                wake_up_interruptible_poll(&ring->wqh, POLLOUT |
                                           POLLWRNORM | POLLWRBAND);
                IFV(printk("%s: pushed %d bytes in the RX ring\n", __func__, len));

		resp = make_rx_response(vif, vif->meta[npo.meta_cons].id,
					status,
					vif->meta[npo.meta_cons].size,
					0);

		RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&vif->rx, ret);

		need_to_notify |= !!ret;

		npo.meta_cons++;
	}

done:
	if (need_to_notify)
		notify_remote_via_irq(vif->rx_irq);
}

void xenmpi_check_rx_xenmpi(struct vmpi_impl_info *vif)
{
	int more_to_do;

	RING_FINAL_CHECK_FOR_REQUESTS(&vif->tx, more_to_do);

	if (more_to_do)
		schedule_work(&vif->tx_worker);
}

static void tx_add_credit(struct vmpi_impl_info *vif)
{
	unsigned long max_burst, max_credit;

	/*
	 * Allow a burst big enough to transmit a jumbo packet of up to 128kB.
	 * Otherwise the interface can seize up due to insufficient credit.
	 */
	max_burst = RING_GET_REQUEST(&vif->tx, vif->tx.req_cons)->size;
	max_burst = min(max_burst, 131072UL);
	max_burst = max(max_burst, vif->credit_bytes);

	/* Take care that adding a new chunk of credit doesn't wrap to zero. */
	max_credit = vif->remaining_credit + vif->credit_bytes;
	if (max_credit < vif->remaining_credit)
		max_credit = ULONG_MAX; /* wrapped: clamp to ULONG_MAX */

	vif->remaining_credit = min(max_credit, max_burst);
}

static void tx_credit_callback(unsigned long data)
{
	struct vmpi_impl_info *vif = (struct vmpi_impl_info *)data;
	tx_add_credit(vif);
	xenmpi_check_rx_xenmpi(vif);
}

static void xenmpi_tx_err(struct vmpi_impl_info *vif,
			  struct xen_mpi_tx_request *txp, RING_IDX end)
{
	RING_IDX cons = vif->tx.req_cons;

	do {
		make_tx_response(vif, txp, XEN_NETIF_RSP_ERROR);
		if (cons == end)
			break;
		txp = RING_GET_REQUEST(&vif->tx, cons++);
	} while (1);
	vif->tx.req_cons = cons;
}

static void xenmpi_fatal_tx_err(struct vmpi_impl_info *vif)
{
        printk("%s: fatal error\n", __func__);
}

static struct page *xenmpi_alloc_page(struct vmpi_impl_info *vif,
				      u16 pending_idx)
{
	struct page *page;

	page = alloc_page(GFP_ATOMIC|__GFP_COLD);
	if (!page)
		return NULL;
	vif->mmap_pages[pending_idx] = page;

	return page;
}

static int xenmpi_tx_check_gop(struct vmpi_impl_info *vif,
                               struct vmpi_buffer *buf,
			       struct gnttab_copy **gopp)
{
	struct gnttab_copy *gop = *gopp;
	u16 pending_idx = *((u16 *)vmpi_buffer_data(buf));
	int err;

	/* Check status of header. */
	err = gop->status;
	if (unlikely(err))
		xenmpi_idx_release(vif, pending_idx, XEN_NETIF_RSP_ERROR);

	*gopp = gop + 1;
	return err;
}

static bool tx_credit_exceeded(struct vmpi_impl_info *vif, unsigned size)
{
	u64 now = get_jiffies_64();
	u64 next_credit = vif->credit_window_start +
		msecs_to_jiffies(vif->credit_usec / 1000);

	/* Timer could already be pending in rare cases. */
	if (timer_pending(&vif->credit_timeout))
		return true;

	/* Passed the point where we can replenish credit? */
	if (time_after_eq64(now, next_credit)) {
		vif->credit_window_start = now;
		tx_add_credit(vif);
	}

	/* Still too big to send right now? Set a callback. */
	if (size > vif->remaining_credit) {
		vif->credit_timeout.data     =
			(unsigned long)vif;
		vif->credit_timeout.function =
			tx_credit_callback;
		mod_timer(&vif->credit_timeout,
			  next_credit);
		vif->credit_window_start = next_credit;

		return true;
	}

	return false;
}

static unsigned xenmpi_tx_build_gops(struct vmpi_impl_info *vif, int budget)
{
	struct gnttab_copy *gop = vif->tx_copy_ops;
        struct vmpi_buffer *buf;

	while ((nr_pending_reqs(vif) + 2 < MAX_PENDING_REQS) &&
	       (vmpi_queue_len(&vif->tx_queue) < budget)) {
		struct xen_mpi_tx_request txreq;
		struct page *page;
		u16 pending_idx;
		RING_IDX cons;
		int work_to_do;
		pending_ring_idx_t pending_cons_idx;

		if (vif->tx.sring->req_prod - vif->tx.req_cons >
		    XEN_NETIF_TX_RING_SIZE) {
			printk(
				   "Impossible number of requests. "
				   "req_prod %d, req_cons %d, size %ld\n",
				   vif->tx.sring->req_prod, vif->tx.req_cons,
				   XEN_NETIF_TX_RING_SIZE);
			xenmpi_fatal_tx_err(vif);
			continue;
		}

		work_to_do = RING_HAS_UNCONSUMED_REQUESTS(&vif->tx);
		if (!work_to_do)
			break;

		cons = vif->tx.req_cons;
		rmb(); /* Ensure that we see the request before we copy it. */
		memcpy(&txreq, RING_GET_REQUEST(&vif->tx, cons), sizeof(txreq));

		/* Credit-based scheduling. */
		if (txreq.size > vif->remaining_credit &&
		    tx_credit_exceeded(vif, txreq.size))
			break;

		vif->remaining_credit -= txreq.size;

		work_to_do--;
		vif->tx.req_cons = ++cons;

		if (unlikely(txreq.size < ETH_HLEN)) {
			printk(
				   "Bad packet size: %d\n", txreq.size);
			xenmpi_tx_err(vif, &txreq, cons);
			break;
		}

		/* No crossing a page as the payload mustn't fragment. */
		if (unlikely((txreq.offset + txreq.size) > PAGE_SIZE)) {
			printk(
				   "txreq.offset: %x, size: %u, end: %lu\n",
				   txreq.offset, txreq.size,
				   (txreq.offset&~PAGE_MASK) + txreq.size);
			xenmpi_fatal_tx_err(vif);
			break;
		}

		pending_cons_idx = pending_index(vif->pending_cons);
		pending_idx = vif->pending_ring[pending_cons_idx];

                buf = vmpi_buffer_create(txreq.size);
		if (unlikely(buf == NULL)) {
			printk(
				   "Can't allocate a vmpi_buffer in start_xmit.\n");
			xenmpi_tx_err(vif, &txreq, cons);
			break;
		}

		/* XXX could copy straight to head */
		page = xenmpi_alloc_page(vif, pending_idx);
		if (!page) {
                        printk("%s: page allocation failed\n", __func__);
                        vmpi_buffer_destroy(buf);
			xenmpi_tx_err(vif, &txreq, cons);
			break;
		}

		gop->source.u.ref = txreq.gref;
		gop->source.domid = vif->domid;
		gop->source.offset = txreq.offset;

		gop->dest.u.gmfn = virt_to_mfn(page_address(page));
		gop->dest.domid = DOMID_SELF;
		gop->dest.offset = txreq.offset;

		gop->len = txreq.size;
		gop->flags = GNTCOPY_source_gref;

		gop++;

		memcpy(&vif->pending_tx_info[pending_idx].req,
		       &txreq, sizeof(txreq));
		vif->pending_tx_info[pending_idx].head = pending_cons_idx;
		*((u16 *)vmpi_buffer_data(buf)) = pending_idx;

                buf->len = txreq.size;

		vif->pending_cons++;

                IFV(printk("%s: built a buffer [len=%d]\n", __func__, (int)buf->len));
                vmpi_queue_push(&vif->tx_queue, buf);

		if ((gop-vif->tx_copy_ops) >= ARRAY_SIZE(vif->tx_copy_ops))
			break;
	}

	return gop - vif->tx_copy_ops;
}


static int xenmpi_tx_submit(struct vmpi_impl_info *vif)
{
	struct gnttab_copy *gop = vif->tx_copy_ops;
        struct vmpi_buffer *buf;
	int work_done = 0;
        struct vmpi_queue *read;
        unsigned int channel;

	while ((buf = vmpi_queue_pop(&vif->tx_queue)) != NULL) {
		struct xen_mpi_tx_request *txp;
		u16 pending_idx;

		pending_idx = *((u16 *)vmpi_buffer_data(buf));
		txp = &vif->pending_tx_info[pending_idx].req;

		/* Check the remap error code. */
		if (unlikely(xenmpi_tx_check_gop(vif, buf, &gop))) {
			printk( "mpiback grant failed.\n");
                        vmpi_buffer_destroy(buf);
			continue;
		}

		memcpy(vmpi_buffer_hdr(buf),
		       (void *)(idx_to_kaddr(vif, pending_idx)|txp->offset),
		       buf->len);

                /* Schedule a response immediately. */
                xenmpi_idx_release(vif, pending_idx,
                                XEN_NETIF_RSP_OKAY);

                channel = vmpi_buffer_hdr(buf)->channel;
                if (unlikely(channel >= VMPI_MAX_CHANNELS)) {
                        printk("%s: bogus channel request: %u\n",
                                __func__, channel);
                        channel = 0;
                }

                IFV(printk("%s: submitting len=%d channel=%d\n", __func__,
                                (int)buf->len, channel));

                if (!vif->read_cb) {
                        read = &vif->read[channel];
                        mutex_lock(&read->lock);
                        if (unlikely(vmpi_queue_len(read) >=
                                                VMPI_RING_SIZE)) {
                                vmpi_buffer_destroy(buf);
                        } else {
                                vmpi_queue_push(read, buf);
                        }
                        mutex_unlock(&read->lock);

                        wake_up_interruptible_poll(&read->wqh,
                                        POLLIN |
                                        POLLRDNORM |
                                        POLLRDBAND);
                } else {
                        vif->read_cb(vif->read_cb_data, channel,
                                        vmpi_buffer_data(buf),
                                        buf->len - sizeof(struct vmpi_hdr));
                        vmpi_buffer_destroy(buf);
                }

		work_done++;
	}

	return work_done;
}

/* Called after netfront has transmitted */
int xenmpi_tx_action(struct vmpi_impl_info *vif, int budget)
{
	unsigned nr_gops;
	int work_done;

        IFV(printk("%s\n", __func__));

	if (unlikely(!tx_work_todo(vif)))
		return 0;

	nr_gops = xenmpi_tx_build_gops(vif, budget);

        IFV(printk("%s: %d gops built\n", __func__, nr_gops));

	if (nr_gops == 0)
		return 0;

	gnttab_batch_copy(vif->tx_copy_ops, nr_gops);

	work_done = xenmpi_tx_submit(vif);

        IFV(printk("%s: work_done %d\n", __func__, work_done));

	return work_done;
}

static void xenmpi_idx_release(struct vmpi_impl_info *vif, u16 pending_idx,
			       u8 status)
{
	struct pending_tx_info *pending_tx_info;
	pending_ring_idx_t pending_prod_idx;

	BUG_ON(vif->mmap_pages[pending_idx] == (void *)(~0UL));

	/* Already complete? */
	if (vif->mmap_pages[pending_idx] == NULL)
		return;

	pending_tx_info = &vif->pending_tx_info[pending_idx];

	BUG_ON(vif->pending_ring[pending_index(pending_tx_info->head)]
                != pending_idx);

        make_tx_response(vif, &pending_tx_info->req, status);

        /* Setting any number other than
         * INVALID_PENDING_RING_IDX indicates this slot is
         * starting a new packet / ending a previous packet.
         */
        pending_tx_info->head = 0;

        pending_prod_idx = pending_index(vif->pending_prod++);
        vif->pending_ring[pending_prod_idx] = vif->pending_ring[pending_idx];

	put_page(vif->mmap_pages[pending_idx]);
	vif->mmap_pages[pending_idx] = NULL;

        IFV(printk("%s: relased pidx %d\n", __func__, pending_idx));
}


static void make_tx_response(struct vmpi_impl_info *vif,
			     struct xen_mpi_tx_request *txp,
			     s8       st)
{
	RING_IDX i = vif->tx.rsp_prod_pvt;
	struct xen_mpi_tx_response *resp;
	int notify;

	resp = RING_GET_RESPONSE(&vif->tx, i);
	resp->id     = txp->id;
	resp->status = st;

	vif->tx.rsp_prod_pvt = ++i;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&vif->tx, notify);
	if (notify)
		notify_remote_via_irq(vif->tx_irq);
        IFV(printk("%s: push_response [rsp_prod=%d], [id=%d], [ntfy=%d]\n",
                __func__, i, resp->id, notify));
}

static struct xen_mpi_rx_response *make_rx_response(struct vmpi_impl_info *vif,
					     u16      id,
					     s8       st,
					     u16      size,
					     u16      flags)
{
	RING_IDX i = vif->rx.rsp_prod_pvt;
	struct xen_mpi_rx_response *resp;

	resp = RING_GET_RESPONSE(&vif->rx, i);
	resp->flags      = flags;
	resp->id         = id;
	resp->status     = (s16)size;
	if (st < 0)
		resp->status = (s16)st;

	vif->rx.rsp_prod_pvt = ++i;

	return resp;
}

static inline int rx_work_todo(struct vmpi_impl_info *vif)
{
	return vmpi_ring_pending(vif->write) &&
	       xenmpi_rx_ring_slots_available(vif, vif->rx_last_skb_slots);
}

static inline int tx_work_todo(struct vmpi_impl_info *vif)
{

	if (likely(RING_HAS_UNCONSUMED_REQUESTS(&vif->tx)) &&
	    (nr_pending_reqs(vif) + 2
	     < MAX_PENDING_REQS))
		return 1;

	return 0;
}

void xenmpi_unmap_frontend_rings(struct vmpi_impl_info *vif)
{
	if (vif->tx.sring)
		xenbus_unmap_ring_vfree(xenmpi_to_xenbus_device(vif),
					vif->tx.sring);
	if (vif->rx.sring)
		xenbus_unmap_ring_vfree(xenmpi_to_xenbus_device(vif),
					vif->rx.sring);
}

int xenmpi_map_frontend_rings(struct vmpi_impl_info *vif,
			      grant_ref_t tx_ring_ref,
			      grant_ref_t rx_ring_ref)
{
	void *addr;
	struct xen_mpi_tx_sring *txs;
	struct xen_mpi_rx_sring *rxs;

	int err = -ENOMEM;

	err = xenbus_map_ring_valloc(xenmpi_to_xenbus_device(vif),
				     tx_ring_ref, &addr);
	if (err)
		goto err;

	txs = (struct xen_mpi_tx_sring *)addr;
	BACK_RING_INIT(&vif->tx, txs, PAGE_SIZE);

	err = xenbus_map_ring_valloc(xenmpi_to_xenbus_device(vif),
				     rx_ring_ref, &addr);
	if (err)
		goto err;

	rxs = (struct xen_mpi_rx_sring *)addr;
	BACK_RING_INIT(&vif->rx, rxs, PAGE_SIZE);

	return 0;

err:
	xenmpi_unmap_frontend_rings(vif);
	return err;
}

void xenmpi_stop_queue(struct vmpi_impl_info *vif)
{
}

static void xenmpi_start_queue(struct vmpi_impl_info *vif)
{
}

int xenmpi_kthread(void *data)
{
	struct vmpi_impl_info *vif = data;

	while (!kthread_should_stop()) {
                IFV(printk("%s: sleeping\n", __func__));
		wait_event_interruptible(vif->wq,
					 rx_work_todo(vif) ||
					 kthread_should_stop());
                IFV(printk("%s: woken up\n", __func__));
		if (kthread_should_stop())
			break;

		if (vmpi_ring_pending(vif->write))
			xenmpi_rx_action(vif);

		if (!vmpi_ring_pending(vif->write))
			xenmpi_start_queue(vif);

		cond_resched();
	}

	return 0;
}

static int __init mpiback_init(void)
{
	if (!xen_domain())
		return -ENODEV;

	return xenmpi_xenbus_init();
}

module_init(mpiback_init);

static void __exit mpiback_fini(void)
{
	xenmpi_xenbus_fini();
}
module_exit(mpiback_fini);

MODULE_LICENSE("Dual BSD/GPL");
