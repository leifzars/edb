/*
 * nodeAccess.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/bTree/nodeAccess.h>

#include <edb/pageMgr/pm_prv.h>

pgIdx_t bpt_getPageIndex(BPT_hndl_t handle, bpt_node_t *node) {
	BPT_t *h = handle;

	/* flush buffer to disk */
	if (h->root == node) {
		return h->indexRootPage[0];
	} else
		return fpm_getPageIndex(h->pm, fpm_pd2p(node));
}
BPT_Result_e bpt_flush(BPT_hndl_t handle, bpt_node_t *buf) {
	BPT_t *h = handle;
	BPT_Result_e err = BPT_OKAY;

	/* flush buffer to disk */
	if (h->root == buf) {
		if (h->rootValid && h->rootModified) {
			err = bpt_rootFlush(handle);
		}
	} else {
		if ((err = (BPT_Result_e) fpm_flushPage(h->pm, bpt_getPageIndex(h, buf))))
			return err;
	}

	return err;
}

BPT_Result_e bpt_returnNode(BPT_hndl_t handle, bpt_node_t *node, bool dirty) {
	BPT_t *h = handle;
	if (h->root == node) {
		if (dirty) {
			h->rootModified = true;
#if EDB_ENABLE_X
			if(!handle->pm->x.active->isWriter){
				edb_trns_makeWriter(handle->pm->x.active);
			}
#endif
		}
	} else {
		void* pg = fpm_pd2p((void*) node);
		fpm_returnPage(h->pm, pg, dirty ? FPM_PGBUF_DIRTY : FPM_PGBUF_NONE);
	}
	return BPT_OKAY;
}
BPT_Result_e bpt_setNodeDirty(BPT_hndl_t handle, bpt_node_t *node) {
	BPT_t *h = handle;
	if (h->root == node) {
		h->rootModified = true;
#if EDB_ENABLE_X
		if(!handle->pm->x.active->isWriter){
			edb_trns_makeWriter(handle->pm->x.active);
		}
#endif
	} else {
		fpm_setPageDirty(h->pm, fpm_pd2p((void*) node));
	}
	return BPT_OKAY;
}

BPT_Result_e bpt_getNode(BPT_hndl_t handle, bpt_address_t adr, bpt_node_t **b) {
	BPT_t *h = handle;
	EDB_result_t err;

	if (adr == (h->indexRootPage[0])) {
		_ASSERT_DEBUG(h->rootValid);
//		if (!h->rootValid) {
//			err = bpt_rootLoad(handle);
//		}
		*b = h->root;
	} else {
		if ((err = FPM_getPage(h->pm, adr, true, (void**) b)))
			return (BPT_Result_e) err;
		*b = fpm_p2pd(*b);

	}

	return BPT_OKAY;
}

BPT_Result_e bpt_syncRoot(BPT_hndl_t handle) {
	BPT_t *h = handle;
	if (!h->rootValid) {
		return bpt_rootLoad(handle);
	} else
		return BPT_OKAY;
}

BPT_Result_e bpt_rootFlush(BPT_hndl_t handle) {
	BPT_t *h = handle;
	BPT_Result_e err;
	void* rPgBuff;
	void* pgBuff;
	/* flush buffer to disk */

	uint8_t* readPtr = (uint8_t*) h->root;
	for (int i = 0; i < BPT_ROOT_SIZE_NUM_OF_SECTORS; i++) {
		if ((err = FPM_getPage(h->pm, h->indexRootPage[i], true, &pgBuff)))
			return err;
		rPgBuff = fpm_p2pd(pgBuff);

		memcpy(rPgBuff, readPtr, h->sectorSize);

		fpm_returnPage(h->pm, pgBuff, FPM_PGBUF_FLUSH);

		readPtr += h->sectorSize;
	}
	h->rootModified = false;
	return BPT_OKAY;
}

BPT_Result_e bpt_rootLoad(BPT_hndl_t handle) {
	BPT_t *h = handle;
	EDB_result_t err;
	void* rPgBuff;
	void* pgBuff;

	uint8_t* rdPtr = (uint8_t*) h->root;
	for (int i = 0; i < BPT_ROOT_SIZE_NUM_OF_SECTORS; i++) {

		if ((err = FPM_getPage(h->pm, h->indexRootPage[i], true, &pgBuff)))
			return (BPT_Result_e) err;
		rPgBuff = fpm_p2pd(pgBuff);

		memcpy(rdPtr, rPgBuff, h->sectorSize);

		fpm_returnPage(h->pm, pgBuff, FPM_PGBUF_NONE);

		rdPtr += h->sectorSize;

	}
	h->rootModified = false;
	h->rootValid = true;
	return BPT_OKAY;
}

#endif
