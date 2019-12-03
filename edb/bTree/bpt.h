#ifndef BPT_H
#define BPT_H

#include <edb/config.h>
#if INCLUDE_EDB

#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "../pageMgr/pm_types.h"
#include <edb/bTree/types.h>
#include <edb/bTree/key.h>


/****************************
 * implementation dependent *
 ****************************/


#define ION_CC_EQ	0
#define ION_CC_GT	1
#define ION_CC_LT	-1

/* typedef int (*ion_bpt_fpm_comparison_t)(const void *key1, const void *key2, unsigned int size); */

/******************************
 * implementation independent *
 ******************************/


/* typedef enum {false, true} bool; */
typedef enum {
	BPT_OKAY = 0,
	BPT_NOT_FOUND = 1024,
	BPT_ERROR,
	BPT_ERROR_DUP_KEYS,
	BPT_ERROR_SECTOR_SIZE,
	BPT_ERROR_MEMORY
} BPT_Result_e;

typedef BPT_t *BPT_hndl_t;

struct EDB_struct_t;

typedef struct {
	/* info for bfOpen() */
	//char					*iName;	/* name of index file */
	struct EDB_struct_t*	pgMgrHndl;
	int						keySize;		/* length, in bytes, of key */
	bool					dupKeys;		/* true if duplicate keys allowed */
	size_t					sectorSize;		/* size of sector on disk */
	bpt_comparison_t		comp;			/* pointer to compare function */

} BPT_initInfo_t;

typedef struct {
	pgIdx_t keyPgIdx;
	uint32_t keyOffset;
	kvp_key_t key;
	pgIdx_t valueOffset; /**< valueOffset in LFB; holds value */
} bpt_cursor_t;
typedef bpt_cursor_t* bpt_crsHndl_t;

/***********************
 * function prototypes *
 ***********************/
BPT_Result_e BPT_open_UInt32(BPT_t* bpt, EDB_Hndl_t pgMgrHndl, pgIdx_t* indexRootPgIdxs, bool dupKeys);
BPT_Result_e BPT_open(BPT_initInfo_t info, BPT_hndl_t btHndl, pgIdx_t* indexRootPgIdxs);

/*
 * input:
 *   info				   info for open
 * output:
 *   handle				 handle to btree, used in subsequent calls
 * returns:
 *   bErrOk				 open was successful
 *   bErrMemory			 insufficient memory
 *   bErrSectorSize		 sector size too small or not 0 mod 4
 *   bErrFileNotOpen		unable to open index file
*/

BPT_Result_e
BPT_close(
	BPT_hndl_t handle
);

BPT_Result_e BPT_flush(BPT_hndl_t handle);
void BPT_cacheInvalidate(BPT_hndl_t handle);
BPT_Result_e BPT_clear(BPT_hndl_t handle);

/*
 * input:
 *   handle				 handle returned by bfOpen
 * returns:
 *   bErrOk				 file closed, resources deleted
*/

BPT_Result_e
BPT_insertKey(
	BPT_hndl_t			handle,
	void						*key,
	bpt_exAddress_t	rec
);

/*
 * input:
 *   handle				 handle returned by bfOpen
 *   key					key to insert
 *   rec					record address
 * returns:
 *   bErrOk				 operation successful
 *   bErrDupKeys			duplicate keys (and info.dupKeys = false)
 * notes:
 *   If dupKeys is false, then all records inserted must have a
 *   unique key.  If dupkeys is true, then duplicate keys are
 *   allowed, but they must all have unique record addresses.
 *   In this case, record addresses are included in internal
 *   nodes to generate a "unique" key.
*/

BPT_Result_e
BPT_updateKey(
	BPT_hndl_t			handle,
	void						*key,
	bpt_exAddress_t	rec
);

/*
 * input:
 *   handle				 handle returned by bfOpen
 *   key					key to update
 *   rec					record address
 * returns:
 *   bErrOk				 operation successful
 *   bErrDupKeys			duplicate keys (and info.dupKeys = false)
 * notes:
 *   If dupKeys is false, then all records updated must have a
 *   unique key.  If dupkeys is true, then duplicate keys are
 *   allowed, but they must all have unique record addresses.
 *   In this case, record addresses are included in internal
 *   nodes to generate a "unique" key.
*/

BPT_Result_e
BPT_deleteKey(
	BPT_hndl_t			handle,
	void						*key,
	bpt_exAddress_t	*rec
);

/*
 * input:
 *   handle				 handle returned by bfOpen
 *   key					key to delete
 *   rec					record address of key to delete
 * output:
 *   rec					record address deleted
 * returns:
 *   bErrOk				 operation successful
 *   bErrKeyNotFound		key not found
 * notes:
 *   If dupKeys is false, all keys are unique, and rec is not used
 *   to determine which key to delete.  If dupKeys is true, then
 *   rec is used to determine which key to delete.
*/
BPT_Result_e BPT_get(BPT_hndl_t handle, uint32_t key, pgIdx_t* val) ;
BPT_Result_e BPT_count(BPT_hndl_t handle, uint32_t* out_ct );

BPT_Result_e BPT_findKey(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl);
BPT_Result_e BPT_findKeyValue(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl, pgIdx_t val);
BPT_Result_e BPT_getKeyValue(BPT_hndl_t handle, uint32_t key, pgIdx_t val);

/*
 * input:
 *   handle				 handle returned by bfOpen
 *   key					key to find
 * output:
 *   rec					record address
 * returns:
 *   bErrOk				 operation successful
 *   bErrKeyNotFound		key not found
*/

BPT_Result_e
BPT_findfFirstGreaterOrEqual(
	BPT_hndl_t			handle,
	void						*key,
	bpt_crsHndl_t crsHndl
);
BPT_Result_e BPT_findLastLessOrEqual(BPT_hndl_t handle, void *key, bpt_crsHndl_t crsHndl) ;

/*
 * input:
 *   handle				 handle returned by bfOpen
 *   key					key to find
 * output:
 *   mkey				   key associated with the found offset
 *   rec					record address of least element greater than or equal to
 * returns:
 *   bErrOk				 operation successful
*/

BPT_Result_e
BPT_findFirstKey(
	BPT_hndl_t			handle,
	bpt_crsHndl_t crsHndl
);

/*
 * input:
 *   handle				 handle returned by bfOpen
 * output:
 *   key					first key in sequential set
 *   rec					record address
 * returns:
 *   bErrOk				 operation successful
 *   bErrKeyNotFound		key not found
*/

BPT_Result_e
BPT_findLastKey(
	BPT_hndl_t			handle,
	bpt_crsHndl_t crsHndl
);

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

BPT_Result_e
BPT_findNextKey(
	BPT_hndl_t			handle,
	bpt_crsHndl_t crsHndl
);
BPT_Result_e BPT_findPrevKey(BPT_hndl_t handle, bpt_crsHndl_t crsHndl);


#endif
#endif /*BPT_H*/
