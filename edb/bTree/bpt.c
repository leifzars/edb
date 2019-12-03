#include <edb/config.h>
#if INCLUDE_EDB

#include "stdlib.h"
#include "string.h"

#include <edb/bTree/nodeAccess.h>
/*************
 * internals *
 *************/

/*
 *  algorithm:
 *	A B+tree implementation, with keys stored in internal nodes,
 *	and keys/record addresses stored in leaf nodes.  Each node is
 *	one sector in length, except the root node whose length is
 *	3 sectors.  When traversing the tree to insert a key, full
 *	children are adjusted to make room for possible new entries.
 *	Similarly, on deletion, half-full nodes are adjusted to allow for
 *	possible deleted entries.  Adjustments are first done by
 *	examining 2 nearest neighbors at the same level, and redistibuting
 *	the keys if possible.  If redistribution won't solve the problem,
 *	nodes are split/joined as needed.  Typically, a node is 3/4 full.
 *	On insertion, if 3 nodes are full, they are split into 4 nodes,
 *	each 3/4 full.  On deletion, if 3 nodes are 1/2 full, they are
 *	joined to create 2 nodes 3/4 full.
 *
 *	A LRR (least-recently-read) buffering scheme for nodes is used to
 *	simplify storage management, and, assuming some locality of reference,
 *	improve performance.
 *
 *	To simplify matters, both internal nodes and leafs contain the
 *	same fields.
 *
 */

/* Allocate buflist.
 * During insert/delete, need simultaneous access to 7 buffers:
 *  - 4 adjacent child bufs
 *  - 1 parent buf
 *  - 1 next sequential link
 *  - 1 lastGE
 */

/*
 * Allocate bufs.
 * We need space for the following:
 *  - bufCt buffers, of size sectorSize
 *  - 1 buffer for root, of size 3*sectorSize
 *  - 1 buffer for gbuf, size 3*sectorsize + 2 extra keys
 *	to allow for LT pointers in last 2 nodes when gathering 3 full nodes
 */

/* macros for addressing fields */

typedef enum ION_BPP_MODE {
	MODE_FIRST, MODE_MATCH, MODE_FGEQ, MODE_LLEQ
} ion_bpt_fpm_mode_e;

static BPT_Result_e searchNonLeaf_wr(BPT_t *h, bpt_node_t **cNode, void *key, bpt_exAddress_t rec,
		ion_bpt_fpm_mode_e mode);
static BPT_Result_e searchNonLeaf(BPT_t *h, bpt_node_t* cNode, void *key, bpt_exAddress_t rec, ion_bpt_fpm_mode_e mode,
		bpt_node_t** out_nNode, bpt_key_t** out_mkey, int* out_cc);
static BPT_Result_e findKey(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl, pgIdx_t val,
		ion_bpt_fpm_mode_e leafMode);

static int search(BPT_hndl_t handle, bpt_node_t *node, void *key, bpt_exAddress_t rec, bpt_key_t **mkey,
		ion_bpt_fpm_mode_e mode) {
	/*
	 * input:
	 *   p					  pointer to node
	 *   key					key to find
	 *   rec					record address (dupkey only)
	 * output:
	 *   k					  pointer to ion_bpt_fpm_key_t info
	 * returns:
	 *   CC_EQ				  key = mkey
	 *   CC_LT				  key < mkey
	 *   CC_GT				  key > mkey
	 */
	BPT_t *h = handle;
	int cc; /* condition code */
	int m; /* midpoint of search */
	int lb; /* lower-bound of binary search */
	int ub; /* upper-bound of binary search */
	bool foundDup; /* true if found a duplicate key */

	/* scan current node for key using binary search */
	foundDup = false;
	lb = 0;
	ub = ct(node) - 1;

	while (lb <= ub) {
		m = (lb + ub) / 2;
		*mkey = fkey(node) + ks(m);

		cc = h->comp(key, key(*mkey), (ion_key_size_t) (h->keySize));

		if ((cc < 0) || ((cc == 0) && (MODE_FGEQ == mode))) {
			/* key less than key[m] */

			ub = m - 1;
		} else if ((cc > 0) || ((cc == 0) && (MODE_LLEQ == mode))) {
			/* key greater than key[m] */

			lb = m + 1;
		} else {

			/* keys match */
			if (h->dupKeys) {
				switch (mode) {
				case MODE_FIRST:
					/* backtrack to first key */
					//Should only be called from a leaf
					ub = m - 1;
					if (lb > ub)
						return ION_CC_EQ;
					foundDup = true;
					break;

				case MODE_MATCH:

					/* rec's must also match */
					if (rec < rec(*mkey)) {
						ub = m - 1;
						cc = ION_CC_LT;
					} else if (rec > rec(*mkey)) {
						lb = m + 1;
						cc = ION_CC_GT;
					} else {
						return ION_CC_EQ;
					}

					break;

				case MODE_FGEQ:
				case MODE_LLEQ: /* nop */
					break;
				}
			} else {
				return cc;
			}
		}

	}

	if (ct(node) == 0) {
		/* empty list */
		*mkey = fkey(node);
		return ION_CC_LT;
	}

	if (h->dupKeys && (mode == MODE_FIRST) && foundDup) {
		/* next key is first key in set of duplicates */
		*mkey += ks(1);
		return ION_CC_EQ;
	}

	if (MODE_LLEQ == mode) {
		*mkey = fkey(node) + ks(ub + 1);
		cc = h->comp(key, key(*mkey), (ion_key_size_t) (h->keySize));

		if ((ub == ct(node) - 1) || ((ub != -1) && (cc <= 0))) {
			*mkey = fkey(node) + ks(ub);
			cc = h->comp(key, key(*mkey), (ion_key_size_t) (h->keySize));
		}

		return cc;
	}

	if (MODE_FGEQ == mode) {
		*mkey = fkey(node) + ks(lb);
		cc = h->comp(key, key(*mkey), (ion_key_size_t) (h->keySize));

		if ((lb < ct(node) - 1) && (cc < 0)) {
			*mkey = fkey(node) + ks(lb + 1);
			cc = h->comp(key, key(*mkey), (ion_key_size_t) (h->keySize));
		}

		return cc;
	}

	/* didn't find key */
	return cc;
}

BPT_Result_e searchNonLeaf_wr(BPT_t *h, bpt_node_t **cNode, void *key, bpt_exAddress_t rec, ion_bpt_fpm_mode_e mode) {
	int searchR;
	bpt_node_t *nNode;
	bpt_key_t *mkey;
	BPT_Result_e rc;

	searchR = search((BPT_hndl_t) h, *cNode, key, rec, &mkey, mode);
	if (searchR < 0) {
		if ((rc = bpt_getNode((BPT_hndl_t) h, childLT(mkey), &nNode)) != 0) {
			return rc;
		}
	} else {
		if ((rc = bpt_getNode((BPT_hndl_t) h, childGE(mkey), &nNode)) != 0) {
			return rc;
		}
	}

	bpt_returnNode((BPT_hndl_t) h, *cNode, false);
	*cNode = nNode;

	return BPT_OKAY;
}

BPT_Result_e searchNonLeaf(BPT_t *h, bpt_node_t* cNode, void *key, bpt_exAddress_t rec, ion_bpt_fpm_mode_e mode,
		bpt_node_t** out_nNode, bpt_key_t** out_mkey, int* out_cc) {
	BPT_Result_e rc;

	*out_cc = search((BPT_hndl_t) h, cNode, key, rec, out_mkey, mode);
	if (*out_cc < 0) {
		if ((rc = bpt_getNode((BPT_hndl_t) h, childLT(*out_mkey), out_nNode)) != 0) {
			return rc;
		}
	} else {
		if ((rc = bpt_getNode((BPT_hndl_t) h, childGE(*out_mkey), out_nNode)) != 0) {
			return rc;
		}
	}

	return BPT_OKAY;
}

static BPT_Result_e scatterRoot(BPT_hndl_t handle) {
	BPT_t *h = handle;
	bpt_node_t *gbuf;
	bpt_node_t *root;

	/* scatter gbuf to root */

	root = h->root;
	gbuf = h->gbuf;
	memcpy(fkey(root), fkey(gbuf), ks(ct(gbuf)));
	childLT(fkey(root)) = childLT(fkey(gbuf));
	ct(root) = ct(gbuf);
	leaf(root) = leaf(gbuf);
	return BPT_OKAY;
}

static BPT_Result_e scatter(BPT_hndl_t handle, bpt_node_t *pnode, bpt_key_t *pkey, int is, bpt_node_t **tmp) {
	BPT_t *h = handle;
	bpt_node_t *gbuf; /* gather buf */
	bpt_key_t *gkey; /* gather buf key */
	BPT_Result_e rc; /* return code */
	bpt_address_t pgIdx;
	int iu; /* number of tmp's used */
	int k0Min; /* min #keys that can be mapped to tmp[0] */
	int knMin; /* min #keys that can be mapped to tmp[1..3] */
	int k0Max; /* max #keys that can be mapped to tmp[0] */
	int knMax; /* max #keys that can be mapped to tmp[1..3] */
	int sw; /* shift width */
	int len; /* length of remainder of buf */
	int base; /* base count distributed to tmps */
	int extra; /* extra counts */
	int ct;
	int i;

	/*
	 * input:
	 *   pnode				   parent buffer of gathered keys
	 *   pkey				   where we insert a key if needed in parent
	 *   is					 number of supplied tmps
	 *   tmp					array of tmp's to be used for scattering
	 * output:
	 *   tmp					array of tmp's used for scattering
	 */

	/* scatter gbuf to tmps, placing 3/4 max in each tmp */

	gbuf = h->gbuf;
	gkey = fkey(gbuf);
	ct = ct(gbuf);

	/****************************************
	 * determine number of tmps to use (iu) *
	 ****************************************/
	iu = is;

	/* determine limits */
	if (leaf(gbuf)) {
		/* minus 1 to allow for insertion */
		k0Max = h->maxCt - 1;
		knMax = h->maxCt - 1;
		/* plus 1 to allow for deletion */
		k0Min = (h->maxCt / 2) + 1;
		knMin = (h->maxCt / 2) + 1;
	} else {
		/* can hold an extra gbuf key as it's translated to a LT pointer */
		k0Max = h->maxCt - 1;
		knMax = h->maxCt;
		k0Min = (h->maxCt / 2) + 1;
		knMin = ((h->maxCt + 1) / 2) + 1;
	}

//	/* calculate iu, number of tmps to use */
	int32_t newPgs = 0;
	void *newPgPtrs[4];

	/*Alloc new pages first
	 * on error we can fail without undoing any linking.
	 */
	while (1) {
		if ((iu == 0) || (ct > (k0Max + (iu - 1) * knMax))) {
			if ((rc = fpm_getEmptyPage(h->pm, EDB_PM_PT_INDEX, &pgIdx, (void**) &newPgPtrs[iu])) != 0) {
				while (iu--) {
					if (iu >= is) {
						pgIdx = fpm_getPageIndex(h->pm, newPgPtrs[iu]);
						edb_freePage(h->pm, pgIdx);
					}
				}
				return rc;
			}
			newPgs++;
			iu++;
		} else {
			break;
		}
	}

	iu = is;

	while (1) {
		if ((iu == 0) || (ct > (k0Max + (iu - 1) * knMax))) {
			/* add a buffer */
			((edb_pm_pageHeader_t*) newPgPtrs[iu])->type = EDB_PM_PT_INDEX;
			tmp[iu] = fpm_p2pd(newPgPtrs[iu]);

			/* update sequential links */
			if (leaf(gbuf)) {
				/* adjust sequential links */
				if (iu == 0) {
					/* no tmps supplied when splitting root for first time */
					prev(tmp[0])= 0;
					next(tmp[0]) = 0;
				}
				else {
					prev(tmp[iu]) = bpt_getPageIndex(h, tmp[iu - 1]);
					next(tmp[iu]) = next(tmp[iu - 1]);
					next(tmp[iu - 1]) = bpt_getPageIndex(h, tmp[iu]); //newNodePgIdx
				}
			}

			iu++;
			handle->stats.nNodesIns++;
		} else if ((iu > 1) && (ct < (k0Min + (iu - 1) * knMin))) {

//			for (uint32_t f = 0; f < iu; f++) {
//				pgIdx = bpt_getPageIndex(h, tmp[f]);
//				EDB_PRINT_I("DEL Page %d    %d <- %d -> %d", pgIdx, tmp[f]->prev, pgIdx,   tmp[f]->next);
//			}
//i think i can delete the middle node, and save a node read on linkage update???
			iu--;

			/* adjust sequential links */
			if (leaf(gbuf)
			&& bpt_getPageIndex(h, tmp[iu - 1]) != h->indexRootPage[0]) {
				next(tmp[iu - 1])= next(tmp[iu]);
			}

			/* del a buffer */
			pgIdx = bpt_getPageIndex(h, tmp[iu]);
			//EDB_PRINT_I("DEL IDX Page %d ", pgIdx);
			bpt_returnNode(handle, tmp[iu], true);
			edb_freePage(h->pm, pgIdx);

			handle->stats.nNodesDel++;
		} else {
			break;
		}
	}

	/* establish count for each tmp used */
	base = ct / iu;
	extra = ct % iu;

	for (i = 0; i < iu; i++) {
		int n;

		n = base;

		/* distribute extras, one at a time */
		/* don't do to 1st node, as it may be internal and can't hold it */
		if (i && extra) {
			n++;
			extra--;
		}

		ct(tmp[i])= n;
	}

	/**************************************
	 * update sequential links and parent *
	 **************************************/
	if (iu != is) {
		/* link last node to next */
		if (leaf(gbuf) && next(tmp[iu - 1])) {
			bpt_node_t *node;

			if ((rc = bpt_getNode(handle, next(tmp[iu - 1]), &node)) != 0) {
				return rc;
			}

			prev(node) = bpt_getPageIndex(h, tmp[iu - 1]);

			if ((rc = bpt_returnNode(handle, node, true)) != 0) {
				return rc;
			}
		}

		/* shift keys in parent */
		sw = ks(iu - is);

		if (sw < 0) {
			len = ks(ct(pnode)) - (pkey - fkey(pnode)) + sw;
			memmove(pkey, pkey - sw, len);
		} else {
			len = ks(ct(pnode)) - (pkey - fkey(pnode));
			memmove(pkey + sw, pkey, len);
		}

		/* don't count LT buffer for empty parent */
		if (ct(pnode)) {
			ct(pnode) += iu - is;
		} else {
			ct(pnode) += iu - is - 1;
		}

//		for (uint32_t f = 0; f < iu; f++) {
//			pgIdx = bpt_getPageIndex(h, tmp[f]);
//			EDB_PRINT_I(" DEL Page %d    %d <- %d -> %d", pgIdx,  tmp[f]->prev, pgIdx,  tmp[f]->next);
//		}

	}

	/*******************************
	 * distribute keys to children *
	 *******************************/
	for (i = 0; i < iu; i++) {
		/* update LT pointer and parent nodes */
		if (leaf(gbuf)) {
			/* update LT, tmp[i] */
			childLT(fkey(tmp[i])) = 0;

			/* update parent */
			if (i == 0) {
				childLT(pkey) = bpt_getPageIndex(h, tmp[i]);
			} else {
				memcpy(pkey, gkey, ks(1));
				childGE(pkey) = bpt_getPageIndex(h, tmp[i]);
				pkey += ks(1);
			}
		} else {
			if (i == 0) {
				/* update LT, tmp[0] */
				childLT(fkey(tmp[i])) = childLT(gkey);
				/* update LT, parent */
				childLT(pkey) = bpt_getPageIndex(h, tmp[i]);
			} else {
				/* update LT, tmp[i] */
				childLT(fkey(tmp[i])) = childGE(gkey);
				/* update parent key */
				memcpy(pkey, gkey, ks(1));
				childGE(pkey) = bpt_getPageIndex(h, tmp[i]);
				gkey += ks(1);
				pkey += ks(1);
				ct(tmp[i])--;
			}
		}

		/* install keys, tmp[i] */
		memcpy(fkey(tmp[i]), gkey, ks(ct(tmp[i])));
		leaf(tmp[i]) = leaf(gbuf);

		gkey += ks(ct(tmp[i]));
	}

	leaf(pnode) = false;

	/************************
	 * write modified nodes *
	 ************************/
	if ((rc = bpt_setNodeDirty(handle, pnode)) != 0) {
		return rc;
	}

	for (i = 0; i < iu; i++) {
		if ((rc = bpt_returnNode(handle, tmp[i], true)) != 0) {
			return rc;
		}
	}

	return BPT_OKAY;
}

static BPT_Result_e gatherRoot(BPT_hndl_t handle) {
	BPT_t *h = handle;
	bpt_node_t *gbuf;
	bpt_node_t *root;

	/* gather root to gbuf */
	root = h->root;
	gbuf = h->gbuf;
	memcpy(gbuf, root, 3 * h->sectorSize);
	leaf(gbuf) = leaf(root);
	ct(root) = 0;
	return BPT_OKAY;
}

static BPT_Result_e gather(BPT_hndl_t handle, bpt_node_t *pnode, bpt_key_t **pkey, bpt_node_t **tmp) {
	BPT_t *h = handle;
	BPT_Result_e rc; /* return code */
	bpt_node_t *gbuf;
	bpt_key_t *gkey;

	/*
	 * input:
	 *   pnode				   parent buffer
	 *   pkey				   pointer to match key in parent
	 * output:
	 *   tmp					buffers to use for scatter
	 *   pkey				   pointer to match key in parent
	 * returns:
	 *   bErrOk				 operation successful
	 * notes:
	 *   Gather 3 buffers to gbuf.  Setup for subsequent scatter by
	 *   doing the following:
	 *	 - setup tmp buffer array for scattered buffers
	 *	 - adjust pkey to point to first key of 3 buffers
	 */

	/* find 3 adjacent buffers */
	if (*pkey == lkey(pnode)) {
		*pkey -= ks(1);
	}

	rc = bpt_getNode(handle, childLT(*pkey), &tmp[0]);

	if (!rc)
		rc = bpt_getNode(handle, childGE(*pkey), &tmp[1]);

	if (!rc)
		rc = bpt_getNode(handle, childGE(*pkey + ks(1)), &tmp[2]);

	if (rc == BPT_OKAY) {
		/* gather nodes to gbuf */
		gbuf = h->gbuf;
		gkey = fkey(gbuf);

		/* tmp[0] */
		childLT(gkey) = childLT(fkey(tmp[0]));
		memcpy(gkey, fkey(tmp[0]), ks(ct(tmp[0])));
		gkey += ks(ct(tmp[0]));
		ct(gbuf) = ct(tmp[0]);

		/* tmp[1] */
		if (!leaf(tmp[1])) {
			memcpy(gkey, *pkey, ks(1));
			childGE(gkey) = childLT(fkey(tmp[1]));
			ct(gbuf)++;
			gkey += ks(1);
		}

		memcpy(gkey, fkey(tmp[1]), ks(ct(tmp[1])));
		gkey += ks(ct(tmp[1]));
		ct(gbuf) += ct(tmp[1]);

		/* tmp[2] */
		if (!leaf(tmp[2])) {
			memcpy(gkey, *pkey + ks(1), ks(1));
			childGE(gkey) = childLT(fkey(tmp[2]));
			ct(gbuf)++;
			gkey += ks(1);
		}

		memcpy(gkey, fkey(tmp[2]), ks(ct(tmp[2])));
		ct(gbuf) += ct(tmp[2]);

		leaf(gbuf) = leaf(tmp[0]);
	}

//	if (tmp[0])
//		bpt_returnNode(handle, tmp[0], true);
//	if (tmp[1])
//		bpt_returnNode(handle, tmp[1], true);
//	if (tmp[2])
//		bpt_returnNode(handle, tmp[2], true);

	return rc;
}
BPT_Result_e BPT_open_UInt32(BPT_t* bpt, EDB_Hndl_t pgMgrHndl, pgIdx_t* indexRootPgIdxs, bool dupKeys) {
	BPT_initInfo_t info;
	info.keySize = sizeof(uint32_t);
	info.dupKeys = dupKeys;
	info.sectorSize = EDB_PM_PAGE_DATA_SIZE & ~0x7; //496; //256;
	info.comp = bpt_compare_uint32;
	info.pgMgrHndl = pgMgrHndl;

	BPT_Result_e bErr = BPT_open(info, bpt, indexRootPgIdxs);

	return bErr;
}

BPT_Result_e BPT_open(BPT_initInfo_t info, BPT_hndl_t btHndl, pgIdx_t* indexRootPgIdxs) {
	//BPT_Result_e rc; /* return code */
	EDB_result_t err;
	int maxCt; /* maximum number of keys in a node */
	bpt_node_t *p;

	if ((info.sectorSize < sizeof(BPT_t)) || (0 != info.sectorSize % 4)) {
		return BPT_ERROR_SECTOR_SIZE;
	}

	/* determine sizes and offsets */
	/* leaf/n, prev, next, [childLT,key,rec]... childGE */
	/* ensure that there are at least 3 children/parent for gather/scatter */
	maxCt = info.sectorSize - (sizeof(bpt_node_t) - sizeof(bpt_key_t));
	maxCt /= sizeof(bpt_address_t) + info.keySize + sizeof(bpt_exAddress_t);

	if (maxCt < 6) {
		return BPT_ERROR_SECTOR_SIZE;
	}

	memset(btHndl, 0, sizeof(BPT_t));

	btHndl->keySize = info.keySize;
	btHndl->dupKeys = info.dupKeys;
	btHndl->sectorSize = info.sectorSize;
	btHndl->comp = info.comp;

	/* childLT, key, rec */
	btHndl->ks = sizeof(bpt_address_t) + btHndl->keySize + sizeof(bpt_exAddress_t);
	btHndl->maxCt = maxCt;

	/*
	 * Allocate bufs.
	 * We need space for the following:
	 *  - 1 buffer for root, of size 3*sectorSize
	 *  - 1 buffer for gbuf, size 3*sectorsize + 2 extra keys
	 *	to allow for LT pointers in last 2 nodes when gathering 3 full nodes
	 */
	int size = 6 * btHndl->sectorSize + 2 * btHndl->ks;
	if ((btHndl->malloc2 = _OS_MAlloc(size)) == NULL) {
		return BPT_ERROR_MEMORY;
	}
	memset(btHndl->malloc2, 0, size);

	p = btHndl->malloc2;
	btHndl->root = p;
	p = (bpt_node_t *) ((char *) p + 3 * btHndl->sectorSize);
	btHndl->gbuf = p;/* done last to include extra 2 keys */

	btHndl->pm = info.pgMgrHndl;
	/* initialize root */
	if (indexRootPgIdxs[0] != 0) {
		memcpy(btHndl->indexRootPage, indexRootPgIdxs, sizeof(pgIdx_t) * BPT_ROOT_SIZE_NUM_OF_SECTORS);
		bpt_rootLoad(btHndl);
		//if ((rc = bpt_getNode(btHndl, indexRootPgIdxs[0], &btHndl->root)) != 0) {
		//	return rc;
		//}
	} else {
		for (int j = 0; j < BPT_ROOT_SIZE_NUM_OF_SECTORS; j++) {
			void* pgBuf;
			err = fpm_getEmptyPage(btHndl->pm, EDB_PM_PT_INDEX, (pgIdx_t*) &btHndl->indexRootPage[j], &pgBuf);
			if (err) {
				while (j--) {
					edb_freePage(btHndl->pm, btHndl->indexRootPage[j]);
				}
				return err;
			}

			fpm_returnPage(btHndl->pm, pgBuf, FPM_PGBUF_DIRTY);
		}

		memcpy(indexRootPgIdxs, btHndl->indexRootPage, sizeof(pgIdx_t) * BPT_ROOT_SIZE_NUM_OF_SECTORS);

		leaf(btHndl->root) = 1;
		btHndl->rootModified = 1;
		btHndl->rootValid = true;
		bpt_flush(btHndl, btHndl->root);
	}

	return BPT_OKAY;
}

/*
 * This can not possible work
 * It must leak all child pages
 */
BPT_Result_e BPT_clear(BPT_hndl_t handle) {
	BPT_t *h = handle;
	bpt_syncRoot(handle);

	memset(h->root, 0, h->sectorSize * 3);
	leaf(h->root) = 1;
	h->rootModified = 1;
	bpt_flush(h, h->root);
	return BPT_OKAY;
}
BPT_Result_e BPT_flush(BPT_hndl_t handle) {
	BPT_t *h = handle;
	BPT_Result_e r;

	if ((r = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return r;

	return bpt_flush(h, h->root);
}
void BPT_cacheInvalidate(BPT_hndl_t handle) {
	BPT_t *h = handle;

	h->rootValid = false;
}


BPT_Result_e BPT_close(BPT_hndl_t handle) {
	BPT_t *h = handle;

	if (h == NULL) {
		return BPT_OKAY;
	}

	if (h->root) {
		bpt_flush(handle, h->root);
	}

	if (h->malloc2) {
		_OS_Free(h->malloc2);
	}

	return BPT_OKAY;
}

BPT_Result_e BPT_get(BPT_hndl_t handle, uint32_t key, pgIdx_t* val) {
	bpt_cursor_t c;
	BPT_Result_e r;

	r = findKey(handle, (void*) &key, &c, 0, MODE_FIRST);
	*val = c.valueOffset;

	return r;
}

BPT_Result_e BPT_getKeyValue(BPT_hndl_t handle, uint32_t key, pgIdx_t val) {
	bpt_cursor_t c;
	BPT_Result_e r;

	r = findKey(handle, (void*) &key, &c, val, MODE_MATCH);

	return r;
}

BPT_Result_e BPT_findKeyValue(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl, pgIdx_t val) {
	return findKey(handle, key, crsHndl, val, MODE_MATCH);
}

BPT_Result_e BPT_findKey(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl) {
	return findKey(handle, key, crsHndl, 0, MODE_FIRST);
}

BPT_Result_e findKey(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl, pgIdx_t val, ion_bpt_fpm_mode_e leafMode) {
	bpt_key_t *mkey; /* matched key */
	bpt_node_t *cNode; /* buffer */
	BPT_Result_e rc; /* return code */
	int searchR;

	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	cNode = h->root;

	/* find key, and return address */
	while (1) {
		if (leaf(cNode)) {
			searchR = search(handle, cNode, key, val, &mkey, leafMode);
			if (searchR == 0) {
				crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
				crsHndl->keyOffset = kToIdx(cNode, mkey);
				crsHndl->valueOffset = rec(mkey);
				memcpy(&crsHndl->key, key(mkey), h->keySize);

				rc = BPT_OKAY;
				break;
			} else {
				rc = BPT_NOT_FOUND;
				break;
			}
		} else {
			//For dupKeys it must be MODE_MATCH, not MODE_FIRST
			if ((rc = searchNonLeaf_wr(h, &cNode, key, 0, MODE_MATCH)))
				break;
		}
	}

	if (cNode)
		bpt_returnNode(handle, cNode, false);
	return rc;
}

BPT_Result_e BPT_findfFirstGreaterOrEqual(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl) {
	bpt_key_t *lgeqkey; /* matched key */
	bpt_node_t *cNode; /* buffer */
	BPT_Result_e rc; /* return code */
	int cc;

	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	cNode = h->root;

	/* find key, and return address */
	while (1) {
		if (leaf(cNode)) {
			//not sure this works yet for indexs with dup keys
			if ((cc = search(handle, cNode, key, 0, &lgeqkey, MODE_LLEQ)) > 0) {
				if ((lgeqkey - fkey(cNode)) / (h->ks) == (ct(cNode))) {
					rc = BPT_NOT_FOUND;
					break;
				}

				lgeqkey += ks(1);
			}

			crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
			crsHndl->keyOffset = kToIdx(cNode, lgeqkey);
			crsHndl->valueOffset = rec(lgeqkey);
			//crsHndl->key = key(lgeqkey);
			memcpy(&crsHndl->key, key(lgeqkey), h->keySize);
			memcpy(key, key(lgeqkey), h->keySize);

//			h->curBuf = node;
//			h->curKey = lgeqkey;
//			memcpy(mkey, key(lgeqkey), h->keySize);
//			*rec = rec(lgeqkey);

			rc = BPT_OKAY;
			break;
		} else {
			if ((rc = searchNonLeaf_wr(h, &cNode, key, 0, MODE_LLEQ)))
				break;
//			cc = search(handle, cNode, key, 0, &lgeqkey, MODE_LLEQ);
//
//			if (cc < 0) {
//				if ((rc = bpt_getNode(handle, childLT(lgeqkey), &nNode)) != 0) {
//					break;
//				}
//			} else {
//				if ((rc = bpt_getNode(handle, childGE(lgeqkey), &nNode)) != 0) {
//					break;
//				}
//			}
//			bpt_returnNode(handle, cNode, false);
//			cNode = nNode;
		}
	}

	if (cNode)
		bpt_returnNode(handle, cNode, false);
	return rc;
}


BPT_Result_e BPT_findLastLessOrEqual(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl) {
	bpt_key_t *lgeqkey; /* matched key */
	bpt_node_t *cNode; /* buffer */
	BPT_Result_e rc; /* return code */
	int cc;

	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	cNode = h->root;

	/* find key, and return address */
	while (1) {
		if (leaf(cNode)) {
			//not sure this works yet for indexs with dup keys
			if ((cc = search(handle, cNode, key, 0, &lgeqkey, MODE_LLEQ)) > 0) {
				if ((lgeqkey - fkey(cNode)) / (h->ks) == (ct(cNode))) {
					rc = BPT_NOT_FOUND;
					break;
				}

				//lgeqkey += ks(1);
			}

			crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
			crsHndl->keyOffset = kToIdx(cNode, lgeqkey);
			crsHndl->valueOffset = rec(lgeqkey);
			//crsHndl->key = key(lgeqkey);
			memcpy(&crsHndl->key, key(lgeqkey), h->keySize);
			memcpy(key, key(lgeqkey), h->keySize);

			rc = BPT_OKAY;
			break;
		} else {
			if ((rc = searchNonLeaf_wr(h, &cNode, key, 0, MODE_LLEQ)))
				break;
		}
	}

	if (cNode)
		bpt_returnNode(handle, cNode, false);
	return rc;
}


#define BPT_DUPLICATE_KEY_ORDERED_BY_VALUE		0
typedef struct {
	bpt_address_t lastGE; /* last childGE traversed */
	unsigned int lastGEkey; /* last childGE key traversed */
	bool lastGEvalid; /* true if GE branch taken */
	bool lastLTvalid; /* true if LT branch taken after GE branch */

} lastBranchInfo_t;

void setLastBranchInfo(BPT_t *h, lastBranchInfo_t* lbi, bpt_key_t *mkey, bpt_node_t *node, int cc) {
	if ((cc >= 0) || (mkey != fkey(node))) {
		lbi->lastGEvalid = true;
		lbi->lastLTvalid = false;
		lbi->lastGE = bpt_getPageIndex(h, node);
		lbi->lastGEkey = mkey - fkey(node);

		if (cc < 0) {
			lbi->lastGEkey -= ks(1);
		}
	} else {
		if (lbi->lastGEvalid) {
			lbi->lastLTvalid = true;
		}
	}
}
BPT_Result_e BPT_insertKey(BPT_hndl_t handle, void *key, bpt_exAddress_t rec) {
	int rc; /* return code */
	bpt_key_t *mkey; /* match key */
	int len; /* length to shift */
	int cc; /* condition code */
	bpt_node_t *node, *root;
	bpt_node_t *tmp[4];
	int sResult;
	/* internal node, descend to child */
	bpt_node_t *cbuf; /* child buf */
	unsigned int keyOff;
	lastBranchInfo_t lbi;
	int height; /* height of tree */

	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	h->operationNumber++;

	cbuf = NULL;
	root = h->root;
	lbi.lastGEvalid = false;
	lbi.lastLTvalid = false;

	/* check for full root */
	if (ct(root) == 3 * h->maxCt) {
		/* gather root and scatter to 4 bufs */
		/* this increases b-tree height by 1 */
		if ((rc = gatherRoot(handle)) != 0) {
			return rc;
		}

		if ((rc = scatter(handle, root, fkey(root), 0, tmp)) != 0) {
			return rc;
		}
	}

	node = root;
	height = 0;

	while (1) {
		if (leaf(node)) {
			/* in leaf, and there' room guaranteed */

			if (height > handle->stats.maxHeight) {
				handle->stats.maxHeight = height;
			}

			/* set mkey to point to insertion point */
			sResult = search(handle, node, key, rec, &mkey, MODE_MATCH);
			if (sResult == ION_CC_LT) { /* key < mkey */
				if (!h->dupKeys && (0 != ct(node))
						&& (h->comp(key, mkey, (ion_key_size_t) (h->keySize)) == ION_CC_EQ)) {
					rc = BPT_ERROR_DUP_KEYS;
					break;
				}
			} else if (sResult == ION_CC_EQ) { /* key = mkey */
				rc = BPT_ERROR_DUP_KEYS;
				break;
			} else if (sResult == ION_CC_GT) { /* key > mkey */
				if (!h->dupKeys && (h->comp(key, mkey, (ion_key_size_t) (h->keySize)) == ION_CC_EQ)) {
					rc = BPT_ERROR_DUP_KEYS;
					break;
				}
				mkey += ks(1);
			}

			/* shift items GE key to right */
			keyOff = mkey - fkey(node);
			len = ks(ct(node)) - keyOff;

			if (len) {
				memmove(mkey + ks(1), mkey, len);
			}

			/* insert new key */
			memcpy(key(mkey), key, h->keySize);
			rec(mkey) = rec;
			childGE(mkey) = 0;
			ct(node)++;

			/* if new key is first key, then fixup lastGE key */
			if (!keyOff && lbi.lastLTvalid) {
				bpt_node_t *tbuf;
				bpt_key_t *tkey;

				if ((rc = bpt_getNode(handle, lbi.lastGE, &tbuf)) != 0) {
					break;
				}

				/* tkey = fkey(tbuf) + lastGEkey; */
				tkey = fkey(tbuf);
				memcpy(key(tkey), key, h->keySize);
				rec(tkey) = rec;

				if ((rc = bpt_returnNode(handle, tbuf, true)) != 0) {
					break;
				}
			}

			handle->stats.nKeysIns++;
			rc = BPT_OKAY;
			break;
		} else {

			cbuf = NULL;
			height++;

			/* read child */
			if ((rc = searchNonLeaf(h, node, key, rec, MODE_MATCH, &cbuf, &mkey, &cc)))
				break;

			/* check for room in child */
			if (ct(cbuf) == h->maxCt) {
				/* gather 3 bufs and scatter */
				if ((rc = gather(handle, node, &mkey, tmp)) != 0) {
					break;
				}

				if ((rc = scatter(handle, node, mkey, 3, tmp)) != 0) {
					break;
				}

				if ((rc = bpt_returnNode(handle, cbuf, false)))
					break;

				/* read child */
				if ((rc = searchNonLeaf(h, node, key, rec, MODE_MATCH, &cbuf, &mkey, &cc)))
					break;

			}

			setLastBranchInfo(h, &lbi, mkey, node, cc);

			rc = bpt_returnNode(handle, node, false);
			node = cbuf;
			cbuf = NULL;
		}
	}

	if (cbuf)
		bpt_returnNode(handle, cbuf, false);
	if (node)
		bpt_returnNode(handle, node, rc == BPT_OKAY);

	return rc;
}

BPT_Result_e BPT_updateKey(BPT_hndl_t handle, void *key, bpt_exAddress_t rec) {
	int rc; /* return code */
	bpt_key_t *mkey; /* match key */
	bpt_node_t *node, *root;
	/* internal node, descend to child */
	bpt_node_t *cbuf; /* child buf */
	int height; /* height of tree */
	int sResult;
	BPT_t *h = handle;

	_ASSERT_DEBUG(!h->dupKeys); // does not work.

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	h->operationNumber++;

	cbuf = NULL;
	root = h->root;

	node = root;
	height = 0;

	while (1) {
		if (leaf(node)) {
			/* in leaf, and there' room guaranteed */

			if (height > handle->stats.maxHeight) {
				handle->stats.maxHeight = height;
			}

			/* set mkey to point to update point */
			sResult = search(handle, node, key, rec, &mkey, MODE_MATCH);
			if (sResult != ION_CC_EQ) {
				rc = BPT_NOT_FOUND;
				break;
			}

			/* update key */
			rec(mkey) = rec;
			rc = BPT_OKAY;
			break;
		} else {
			height++;

			if ((rc = searchNonLeaf_wr(h, &node, key, rec, MODE_MATCH)))
				break;
		}
	}

	if (cbuf)
		bpt_returnNode(handle, cbuf, false);
	if (node)
		bpt_returnNode(handle, node, rc == BPT_OKAY);

	return rc;
}

BPT_Result_e BPT_deleteKey(BPT_hndl_t handle, void *key, bpt_exAddress_t *rec) {
	int rc; /* return code */
	bpt_key_t *mkey; /* match key */
	int len; /* length to shift */
	int cc; /* condition code */
	bpt_node_t *node; /* buffer */
	bpt_node_t *tmp[4];
	unsigned int keyOff;
	lastBranchInfo_t lbi;
	bpt_node_t *root;
	bpt_node_t *gbuf;
	/* internal node, descend to child */
	bpt_node_t *cbuf; /* child buf */
	BPT_t *h = handle;
	int sResult;
	bpt_address_t pgIdx;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	h->operationNumber++;

	cbuf = NULL;
	root = h->root;
	gbuf = h->gbuf;
	lbi.lastGEvalid = false;
	lbi.lastLTvalid = false;

	node = root;

	while (1) {
		if (leaf(node)) {
			/* set mkey to point to deletion point */
			sResult = search(handle, node, key, *rec, &mkey, MODE_MATCH);
			if (sResult != ION_CC_EQ) {
				rc = BPT_NOT_FOUND;
				break;
			}
			*rec = rec(mkey);

			/* shift items GT key to left */
			keyOff = mkey - fkey(node);
			len = ks(ct(node) - 1) - keyOff;

			if (len) {
				memmove(mkey, mkey + ks(1), len);
			}

			ct(node)--;

			/* if deleted key is first key, then fixup lastGE key */
			if (!keyOff && lbi.lastLTvalid) {
				bpt_node_t *tbuf;
				bpt_key_t *tkey;

				if ((rc = bpt_getNode(handle, lbi.lastGE, &tbuf)) != 0) {
					break;
				}

				tkey = fkey(tbuf) + lbi.lastGEkey;
				memcpy(key(tkey), mkey, h->keySize);
				rec(tkey) = rec(mkey);

				if ((rc = bpt_returnNode(handle, tbuf, true)) != 0) {
					break;
				}
			}
			rc = BPT_OKAY;
			handle->stats.nKeysDel++;
			break;
		} else {
			cbuf = NULL;

			/* read child */
			if ((rc = searchNonLeaf(h, node, key, *rec, MODE_MATCH, &cbuf, &mkey, &cc)))
				break;

			/* check for room to delete */
			if (ct(cbuf) == h->maxCt / 2) {
				/* gather 3 bufs and scatter */
				if ((rc = gather(handle, node, &mkey, tmp)) != 0) {
					break;
				}

				/* if last 3 bufs in root, and count is low enough... */
				if ((node == root) && (ct(root) == 2) && (ct(gbuf) < (3 * (3 * h->maxCt)) / 4)) {
					/* collapse tree by one level */
					scatterRoot(handle);

					for (int i = 0; i < 3; i++) {
						pgIdx = bpt_getPageIndex(h, tmp[i]);
						bpt_returnNode(handle, tmp[i], true);
						edb_freePage(h->pm, pgIdx);
						//EDB_PRINT_I("DEL IDX Page %d ", pgIdx);
					}

					handle->stats.nNodesDel += 3;
					continue;
				}

				if ((rc = scatter(handle, node, mkey, 3, tmp)) != 0) {
					break;
				}

				if ((rc = bpt_returnNode(handle, cbuf, false)))
					break;

				/* read child */
				if ((rc = searchNonLeaf(h, node, key, *rec, MODE_MATCH, &cbuf, &mkey, &cc)))
					break;
			}

			setLastBranchInfo(h, &lbi, mkey, node, cc);

			rc = bpt_returnNode(handle, node, false);
			node = cbuf;
			cbuf = NULL;
		}
	}

	if (cbuf)
		bpt_returnNode(handle, cbuf, false);
	if (node)
		bpt_returnNode(handle, node, rc == BPT_OKAY);

	return rc;
}

BPT_Result_e BPT_count(BPT_hndl_t handle, uint32_t* out_ct ) {
	BPT_Result_e r;
	BPT_cursor_t cur;
	BPT_record_t record;

	uint32_t ct = 0;

	r = BPT_cursorInit_All(handle, &cur, BPT_PT_ACS);

	while (BPT_cursorNext(&cur, &record) == BPT_CS_CURSOR_ACTIVE) {

		ct++;
	}

	*out_ct = ct;

	return r;
}

BPT_Result_e BPT_findFirstKey(BPT_hndl_t handle, bpt_crsHndl_t crsHndl) {
	BPT_Result_e rc; /* return code */
	bpt_node_t *cNode; /* buffer */
	bpt_node_t *nNode; /* buffer */
	bpt_key_t * fkey;
	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	cNode = h->root;

	while (!leaf(cNode)) {
		rc = bpt_getNode(handle, childLT(fkey(cNode)), &nNode);
		bpt_returnNode(handle, cNode, false);
		if (rc != 0)
			return rc;

		cNode = nNode;
	}

	if (ct(cNode) == 0) {
		rc = BPT_NOT_FOUND;
	} else {

		fkey = fkey(cNode);
		crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
		crsHndl->keyOffset = 0; //kToIdx(node, fkey(node));
		crsHndl->valueOffset = rec(fkey);
		//crsHndl->key = key(fkey);
		memcpy(&crsHndl->key, key(fkey(cNode)), h->keySize);
		rc = BPT_OKAY;
	}
	if (cNode)
		bpt_returnNode(handle, cNode, false);
	return rc;

}

/*
 * input:
 *   handle				 handle returned by bfOpen
 * output:
 *   key					last key in sequential set
 *   rec					record address
 * returns:
 *   bErrOk				 operation successful
 *   bErrKeyNotFound		key not found
 */

BPT_Result_e BPT_findLastKey(BPT_hndl_t handle, bpt_crsHndl_t crsHndl) {
	BPT_Result_e rc; /* return code */
	bpt_node_t *cNode; /* buffer */
	bpt_node_t *nNode; /* buffer */
	bpt_key_t * fkey;
	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	cNode = h->root;

	while (!leaf(cNode)) {
		rc = bpt_getNode(handle, childGE(lkey(cNode)), &nNode);
		bpt_returnNode(handle, cNode, false);
		if (rc != 0)
			return rc;

		cNode = nNode;
	}

	if (ct(cNode) == 0) {
		rc = BPT_NOT_FOUND;
	} else {
		fkey = lkey(cNode);
		crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
		crsHndl->keyOffset = kToIdx(cNode, fkey);
		crsHndl->valueOffset = rec(fkey);
		memcpy(&crsHndl->key, key(fkey(cNode)), h->keySize);
		//memcpy(key, key(fkey(node)), h->keySize);
		rc = BPT_OKAY;
	}
	if (cNode)
		bpt_returnNode(handle, cNode, false);

	return rc;
}

BPT_Result_e BPT_findNextKey(BPT_hndl_t handle, bpt_crsHndl_t crsHndl) {
	BPT_Result_e rc; /* return code */
	bpt_key_t *nkey; /* next key */
	bpt_node_t *cNode; /* buffer */
	bpt_node_t *nNode; /* buffer */

	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	bpt_syncRoot(handle);

	if (crsHndl->keyPgIdx == 0)
		return BPT_NOT_FOUND;

	if ((rc = bpt_getNode(handle, crsHndl->keyPgIdx, &cNode)) != 0)
		return rc;

	if (crsHndl->keyOffset == (uint32_t) (ct(cNode) - 1)) {
		/* current key is last key in leaf node */
		if (next(cNode)) {
			/* fetch next set */
			rc = bpt_getNode(handle, next(cNode), &nNode);
			bpt_returnNode(handle, cNode, false);
			if (rc != 0)
				return rc;

			cNode = nNode;

			nkey = fkey(cNode);
		} else {
			/* no more sets */
			bpt_returnNode(handle, cNode, false);
			return BPT_NOT_FOUND;
		}
	} else {
		/* bump to next key */
		nkey = idxToK(cNode, crsHndl->keyOffset + 1);
	}

	crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
	crsHndl->keyOffset = kToIdx(cNode, nkey);
	crsHndl->valueOffset = rec(nkey);
	//crsHndl->key = key(nkey);
	memcpy(&crsHndl->key, key(nkey), h->keySize);

	if (cNode)
		bpt_returnNode(handle, cNode, false);
	return BPT_OKAY;
}

/*
 * input:
 *   handle				 handle returned by bfOpen
 * output:
 *   key					key found
 *   rec					record address
 * returns:
 *   bErrOk				 operation successful
 *   bErrKeyNotFound		key not found
 */
BPT_Result_e BPT_findPrevKey(BPT_hndl_t handle, bpt_crsHndl_t crsHndl) {
	BPT_Result_e rc; /* return code */
	bpt_key_t *pkey; /* previous key */
	bpt_node_t *cNode; /* buffer */
	bpt_node_t *nNode; /* buffer */

	BPT_t *h = handle;

	if ((rc = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return rc;

	if (crsHndl->keyPgIdx == 0)
		return BPT_NOT_FOUND;

	bpt_syncRoot(handle);

	if ((rc = bpt_getNode(handle, crsHndl->keyPgIdx, &cNode)) != 0)
		return rc;

	if (crsHndl->keyOffset == 0) {
		/* current key is first key in leaf node */
		if (prev(cNode)) {
			/* fetch previous set */
			rc = bpt_getNode(handle, prev(cNode), &nNode);
			bpt_returnNode(handle, cNode, false);
			if (rc != 0)
				return rc;

			cNode = nNode;
			pkey = fkey(cNode) + ks((ct(cNode) - 1));
		} else {
			/* no more sets */
			bpt_returnNode(handle, cNode, false);
			return BPT_NOT_FOUND;
		}
	} else {
		/* bump to previous key */
		pkey = idxToK(cNode, crsHndl->keyOffset - 1);
	}

	crsHndl->keyPgIdx = bpt_getPageIndex(h, cNode);
	crsHndl->keyOffset = kToIdx(cNode, pkey);
	crsHndl->valueOffset = rec(pkey);

	memcpy(&crsHndl->key, key(pkey), h->keySize);

	if (cNode)
		bpt_returnNode(handle, cNode, false);

	return BPT_OKAY;
}
#endif
