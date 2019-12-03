/*
 * nodeAccess.h
 *
 */

#ifndef BPT_NODE_ACCESS_H
#define BPT_NODE_ACCESS_H

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm.h>

/* primitives */
#define bAdr(p)		*(bpt_address_t *) (p)
#define eAdr(p)		*(bpt_exAddress_t *) (p)

/* based on k = &[key,rec,childGE] */
#define childLT(k)	bAdr((char *) k - sizeof(bpt_address_t))
#define key(k)		(k)
#define rec(k)		eAdr((char *) (k) + h->keySize)
#define childGE(k)	bAdr((char *) (k) + h->keySize + sizeof(bpt_exAddress_t))

/* based on b = &ion_bpt_fpm_buffer_t */
#define leaf(p)		p->leaf
#define ct(p)		p->ct
#define next(p)		p->next
#define prev(p)		p->prev
#define fkey(p)		& p->fkey
#define lkey(p)		(fkey(p) + ks((ct(p) - 1)))
#define p(p)		(char *) (p)

/* shortcuts */
#define ks(ct)		((ct) * h->ks)
#define kToIdx(p, k)    (((char *)k- (char *)fkey(p)) / h->ks)
#define idxToK(p, i)    ((char *)fkey(p) + ks(i))




pgIdx_t bpt_getPageIndex(BPT_hndl_t handle, bpt_node_t *node);

BPT_Result_e bpt_syncRoot(BPT_hndl_t handle);
BPT_Result_e bpt_rootLoad(BPT_hndl_t handle);
BPT_Result_e bpt_rootFlush(BPT_hndl_t handle);

BPT_Result_e bpt_flush(BPT_hndl_t handle, bpt_node_t *buf);
void BPT_cacheInvalidate(BPT_hndl_t handle);
//ion_bpt_fpm_err_t bpt_flushAll(ion_bpt_fpm_handle_t handle);
BPT_Result_e bpt_returnNode(BPT_hndl_t handle, bpt_node_t *node, bool dirty);
BPT_Result_e bpt_setNodeDirty(BPT_hndl_t handle, bpt_node_t *node);
BPT_Result_e bpt_getNode(BPT_hndl_t handle, bpt_address_t adr, bpt_node_t **b);


#endif
#endif /* BPT_NODE_ACCESS_H */
