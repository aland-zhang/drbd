#ifndef _DRBD_WRAPPERS_H
#define _DRBD_WRAPPERS_H

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
# error "At least kernel 3.10.0 (with patches) required"
#endif

#include "compat.h"
#include <linux/ctype.h>
#include <linux/net.h>
#include <linux/version.h>
#include <linux/crypto.h>
#include <linux/netlink.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/kernel.h>
#include <linux/kconfig.h>

#ifndef pr_fmt
#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#endif

/* introduced in v3.13-4220-g89a0714106aa */
#ifndef U32_MAX
#define U32_MAX ((u32)~0U)
#endif
#ifndef S32_MAX
#define S32_MAX ((s32)(U32_MAX>>1))
#endif

/* introduced in v3.18-rc3-2-g230fa253df63 */
#ifndef READ_ONCE
#define READ_ONCE ACCESS_ONCE
#endif

/* introduced in v4.3-8058-g71baba4b92dc */
#ifndef __GFP_RECLAIM
#define __GFP_RECLAIM __GFP_WAIT
#endif

#ifndef COMPAT_QUEUE_LIMITS_HAS_DISCARD_ZEROES_DATA
static inline unsigned int queue_discard_zeroes_data(struct request_queue *q)
{
	return 0;
}
#endif

#ifndef FMODE_EXCL
#define FMODE_EXCL 0
#endif

/* introduced in v4.14-rc8-66-gf54bb2ec02c8 */
#ifndef lockdep_assert_irqs_disabled
#define lockdep_assert_irqs_disabled() do { } while (0)
#endif

/* introduced in v5.0-6417-g2bdde670beed */
#ifndef DEFINE_DYNAMIC_DEBUG_METADATA
#define DEFINE_DYNAMIC_DEBUG_METADATA(D, F) do { } while(0)
#define __dynamic_pr_debug(D, F, ...) do { } while(0)
#define DYNAMIC_DEBUG_BRANCH(D) false
#endif

/* introduced in v4.7-11559-g9049fc745300 */
#ifndef DYNAMIC_DEBUG_BRANCH
#define DYNAMIC_DEBUG_BRANCH(descriptor) \
	(unlikely(descriptor.flags & _DPRINTK_FLAGS_PRINT))
#endif

/* How do we tell the block layer to pass down flush/fua? */
#ifndef COMPAT_HAVE_BLK_QUEUE_WRITE_CACHE
static inline void blk_queue_write_cache(struct request_queue *q, bool enabled, bool fua)
{
#if defined(REQ_FLUSH) && !defined(REQ_HARDBARRIER)
/* Linux version 2.6.37 up to 4.7
 * needs blk_queue_flush() to announce driver support */
	blk_queue_flush(q, (enabled ? REQ_FLUSH : 0) | (fua ? REQ_FUA : 0));
#else
/* Older kernels either flag affected bios with BIO_RW_BARRIER, or do not know
 * how to handle this at all. No need to "announce" driver support. */
#endif
}
#endif

#ifndef KREF_INIT
#define KREF_INIT(N) { ATOMIC_INIT(N) }
#endif

#define _adjust_ra_pages(qrap, brap) do { \
	if (qrap != brap) { \
		drbd_info(device, "Adjusting my ra_pages to backing device's (%lu -> %lu)\n", qrap, brap); \
		qrap = brap; \
	} \
} while(0)

#ifndef REQ_NOUNMAP
#define REQ_NOUNMAP 0
#endif

#ifndef REQ_DISCARD
#define REQ_DISCARD 0
#endif

#ifndef REQ_WRITE_SAME
#define REQ_WRITE_SAME 0
#endif

#ifdef COMPAT_HAVE_POINTER_BACKING_DEV_INFO /* >= v4.11 */
#define bdi_from_device(device) (device->ldev->backing_bdev->bd_disk->queue->backing_dev_info)
#define init_bdev_info(bdev_info, drbd_congested, device) do { \
	(bdev_info)->congested_fn = drbd_congested; \
	(bdev_info)->congested_data = device; \
	(bdev_info)->capabilities |= BDI_CAP_STABLE_WRITES; \
} while(0)
#define adjust_ra_pages(q, b) _adjust_ra_pages((q)->backing_dev_info->ra_pages, (b)->backing_dev_info->ra_pages)
#else /* < v4.11 */
#define bdi_rw_congested(BDI) bdi_rw_congested(&BDI)
#define bdi_congested(BDI, BDI_BITS) bdi_congested(&BDI, (BDI_BITS))
#define bdi_from_device(device) (&device->ldev->backing_bdev->bd_disk->queue->backing_dev_info)
#define init_bdev_info(bdev_info, drbd_congested, device) do { \
	(bdev_info).congested_fn = drbd_congested; \
	(bdev_info).congested_data = device; \
	(bdev_info).capabilities |= BDI_CAP_STABLE_WRITES; \
} while(0)
#define adjust_ra_pages(q, b) _adjust_ra_pages((q)->backing_dev_info.ra_pages, (b)->backing_dev_info.ra_pages)
#endif


#if defined(COMPAT_HAVE_BIO_SET_OP_ATTRS) /* compat for Linux before 4.8 {{{2 */
#if (defined(RHEL_RELEASE_CODE /* 7.4 broke our compat detection here */) && \
			LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0))
/* Thank you RHEL 7.4 for backporting just enough to break existing compat code,
 * but not enough to make it work for us without additional compat code.
 */
#define COMPAT_NEED_BI_OPF_AND_SUBMIT_BIO_COMPAT_DEFINES 1
# ifdef COMPAT_HAVE_REQ_OP_WRITE_ZEROES
#  error "unexpectedly defined REQ_OP_WRITE_ZEROES, double check compat wrappers!"
# else
#  define  REQ_OP_WRITE_ZEROES (-3u)
# endif
#endif

#ifndef COMPAT_HAVE_REQ_OP_WRITE_ZEROES
#define REQ_OP_WRITE_ZEROES (-3u)
#endif

#else /* !defined(COMPAT_HAVE_BIO_SET_OP_ATTRS) */
#define COMPAT_NEED_BI_OPF_AND_SUBMIT_BIO_COMPAT_DEFINES 1

enum req_op {
       REQ_OP_READ,				/* 0 */
       REQ_OP_WRITE		= REQ_WRITE,	/* 1 */

       /* Not yet a distinguished op,
	* but identified via FLUSH/FUA flags.
	* If at all. */
       REQ_OP_FLUSH		= REQ_OP_WRITE,

	/* These may be not supported in older kernels.
	 * In that case, the DRBD_REQ_* will be 0,
	 * bio_op() aka. op_from_rq_bits() will never return these,
	 * and we map the REQ_OP_* to something stupid.
	 */
	REQ_OP_DISCARD          = REQ_DISCARD ?: -1,
	REQ_OP_WRITE_SAME       = REQ_WRITE_SAME ?: -2,
	REQ_OP_WRITE_ZEROES     = -3,
};
#define bio_op(bio)                            (op_from_rq_bits((bio)->bi_rw))

static inline int op_from_rq_bits(u64 flags)
{
	if (flags & REQ_DISCARD)
		return REQ_OP_DISCARD;
	else if (flags & REQ_WRITE_SAME)
		return REQ_OP_WRITE_SAME;
	else if (flags & REQ_WRITE)
		return REQ_OP_WRITE;
	else
		return REQ_OP_READ;
}
#endif

/* introduced in v4.4-rc2-61-ga55bbd375d18 */
#ifndef idr_for_each_entry_continue
#define idr_for_each_entry_continue(idp, entry, id)			\
	for (entry = (typeof(entry))idr_get_next((idp), &(id));		\
	     entry;							\
	     ++id, entry = (typeof(entry))idr_get_next((idp), &(id)))
#endif


#ifndef RCU_INITIALIZER
#define RCU_INITIALIZER(v) (typeof(*(v)) *)(v)
#endif

#ifndef list_next_entry
/* introduced in 008208c (v3.13-rc1) */
#define list_next_entry(pos, member) \
        list_entry((pos)->member.next, typeof(*(pos)), member)
#endif

/*
 * v4.12 fceb6435e852 netlink: pass extended ACK struct to parsing functions
 * and some preparation commits introduce a new "netlink extended ack" error
 * reporting mechanism. For now, only work around that here.  As trigger, use
 * NETLINK_MAX_COOKIE_LEN introduced somewhere in the middle of that patchset.
 */
#ifndef NETLINK_MAX_COOKIE_LEN
#include <net/netlink.h>
#define nla_parse_nested(tb, maxtype, nla, policy, extack) \
       nla_parse_nested(tb, maxtype, nla, policy)
#endif

#ifndef BLKDEV_ISSUE_ZEROOUT_EXPORTED
/* Was introduced with 2.6.34 */
extern int blkdev_issue_zeroout(struct block_device *bdev, sector_t sector,
				sector_t nr_sects, gfp_t gfp_mask);
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, flags /* = NOUNMAP */) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP)
#else
/* synopsis changed a few times, though */
#if  defined(BLKDEV_ZERO_NOUNMAP)
/* >= v4.12 */
/* use blkdev_issue_zeroout() as written out in the actual source code.
 * right now, we only use it with flags = BLKDEV_ZERO_NOUNMAP */
#elif  defined(COMPAT_BLKDEV_ISSUE_ZEROOUT_DISCARD)
/* no BLKDEV_ZERO_NOUNMAP as last parameter, but a bool discard instead */
/* still need to define BLKDEV_ZERO_NOUNMAP, to compare against 0 */
#define BLKDEV_ZERO_NOUNMAP 1
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, flags /* = NOUNMAP */) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP, (flags) == 0 /* bool discard */)
#else /* !defined(COMPAT_BLKDEV_ISSUE_ZEROOUT_DISCARD) */
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, discard) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP)
#endif
#endif


#if defined(COMPAT_HAVE_GENERIC_START_IO_ACCT_Q_RW_SECT_PART)
/* void generic_start_io_acct(struct request_queue *q,
 *		int rw, unsigned long sectors, struct hd_struct *part); */
#elif defined(COMPAT_HAVE_GENERIC_START_IO_ACCT_RW_SECT_PART)
/* void generic_start_io_acct(
 *		int rw, unsigned long sectors, struct hd_struct *part); */
#define generic_start_io_acct(q, rw, sect, part) generic_start_io_acct(rw, sect, part)
#define generic_end_io_acct(q, rw, part, start) generic_end_io_acct(rw, part, start)

#elif defined(__disk_stat_inc)
/* too old, we don't care */
#warning "io accounting disabled"
#else

static inline void generic_start_io_acct(struct request_queue *q,
		int rw, unsigned long sectors, struct hd_struct *part)
{
	int cpu;

	cpu = part_stat_lock();
	part_round_stats(cpu, part);
	part_stat_inc(cpu, part, ios[rw]);
	part_stat_add(cpu, part, sectors[rw], sectors);
	(void) cpu; /* The macro invocations above want the cpu argument, I do not like
		       the compiler warning about cpu only assigned but never used... */
	/* part_inc_in_flight(part, rw); */
	{ BUILD_BUG_ON(sizeof(atomic_t) != sizeof(part->in_flight[0])); }
	atomic_inc((atomic_t*)&part->in_flight[rw]);
	part_stat_unlock();
}

static inline void generic_end_io_acct(struct request_queue *q,
		int rw, struct hd_struct *part, unsigned long start_time)
{
	unsigned long duration = jiffies - start_time;
	int cpu;

	cpu = part_stat_lock();
	part_stat_add(cpu, part, ticks[rw], duration);
	part_round_stats(cpu, part);
	/* part_dec_in_flight(part, rw); */
	atomic_dec((atomic_t*)&part->in_flight[rw]);
	part_stat_unlock();
}
#endif /* __disk_stat_inc, COMPAT_HAVE_GENERIC_START_IO_ACCT ... */

/* introduced in dc3f4198eac1 (v4.1-rc3-171) */
#ifndef COMPAT_HAVE_SIMPLE_POSITIVE
#include <linux/dcache.h>
static inline int simple_positive(struct dentry *dentry)
{
        return dentry->d_inode && !d_unhashed(dentry);
}
#endif

#if !(defined(COMPAT_HAVE_SHASH_DESC_ON_STACK) &&    \
      defined COMPAT_HAVE_SHASH_DESC_ZERO)
#include <crypto/hash.h>

/* introduced in a0a77af14117 (v3.17-9284) */
#ifndef COMPAT_HAVE_SHASH_DESC_ON_STACK
#define SHASH_DESC_ON_STACK(shash, ctx)				  \
	char __##shash##_desc[sizeof(struct shash_desc) +	  \
		crypto_shash_descsize(ctx)] CRYPTO_MINALIGN_ATTR; \
	struct shash_desc *shash = (struct shash_desc *)__##shash##_desc
#endif

/* introduced in e67ffe0af4d4 (v4.5-rc1-24) */
#ifndef COMPAT_HAVE_SHASH_DESC_ZERO
#ifndef barrier_data
#define barrier_data(ptr) barrier()
#endif
static inline void shash_desc_zero(struct shash_desc *desc)
{
	/* memzero_explicit(...) */
	memset(desc, 0, sizeof(*desc) + crypto_shash_descsize(desc->tfm));
	barrier_data(desc);
}
#endif
#endif

/* RDMA related */
#ifndef COMPAT_HAVE_IB_CQ_INIT_ATTR
#include <rdma/ib_verbs.h>

struct ib_cq_init_attr {
	unsigned int    cqe;
	int             comp_vector;
	u32             flags;
};

static inline struct ib_cq *
drbd_ib_create_cq(struct ib_device *device,
		  ib_comp_handler comp_handler,
		  void (*event_handler)(struct ib_event *, void *),
		  void *cq_context,
		  const struct ib_cq_init_attr *cq_attr)
{
	return ib_create_cq(device, comp_handler, event_handler, cq_context,
			    cq_attr->cqe, cq_attr->comp_vector);
}

#define ib_create_cq(DEV, COMP_H, EVENT_H, CTX, ATTR) \
	drbd_ib_create_cq(DEV, COMP_H, EVENT_H, CTX, ATTR)
#endif
/* RDMA */

#ifndef COMPAT_HAVE_PROC_CREATE_SINGLE
extern struct proc_dir_entry *proc_create_single(const char *name, umode_t mode,
		struct proc_dir_entry *parent,
		int (*show)(struct seq_file *, void *));
#endif

#ifdef COMPAT_HAVE_MAX_SEND_RECV_SGE
#define MAX_SGE(ATTR) min((ATTR).max_send_sge, (ATTR).max_recv_sge)
#else
#define MAX_SGE(ATTR) (ATTR).max_sge
#endif

#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9
#endif
#ifndef SECTOR_SIZE
#define SECTOR_SIZE (1 << SECTOR_SHIFT)
#endif

/* The declaration of arch_wb_cache_pmem() is in upstream in
   include/linux/libnvdimm.h. In RHEL7.6 it is in drivers/nvdimm/pmem.h.
   The kernel-devel package does not ship drivers/nvdimm/pmem.h.
   Therefore the declaration is here!
   Upstream moved it from drivers/nvdimm/pmem.h to libnvdimm.h with 4.14 */
#if defined(RHEL_MAJOR) && defined(RHEL_MINOR) && defined(CONFIG_ARCH_HAS_PMEM_API)
# if RHEL_MAJOR == 7 && RHEL_MINOR >= 6
void arch_wb_cache_pmem(void *addr, size_t size);
# endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0) && defined(CONFIG_ARCH_HAS_PMEM_API)
void arch_wb_cache_pmem(void *addr, size_t size);
#endif

#endif
