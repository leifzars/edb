/*
 * bpt_types.h
 *
 */

#ifndef BPT_TYPES_H
#define BPT_TYPES_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "../pageMgr/pm_types.h"

typedef char bpt_key_t; // keys entries are treated as char arrays */

typedef pgIdx_t bpt_exAddress_t; // record address for external record */
typedef pgIdx_t bpt_address_t; // record address for btree node */

/**
 @brief		The size (length) of a dictionary key in bytes.
 */
typedef int ion_key_size_t;

/**
 @brief		A dictionary key.
 */
typedef void *ion_key_t;

/* compare two keys and return:
 *	CC_LT	 key1 < key2
 *	CC_GT	 key1 > key2
 *	CC_EQ	 key1 = key2
 */
typedef char (*bpt_comparison_t)(ion_key_t key1, ion_key_t key2, ion_key_size_t size);


typedef struct {
#if 0

	char leaf; // first bit = 1 if leaf */
	uint16_t ct;// count of keys present */

#endif

	unsigned int leaf :1;
	unsigned int ct :15;
	bpt_address_t prev; 		// prev node in sequence (leaf) */
	bpt_address_t next; 		// next node in sequence (leaf) */
	bpt_address_t childLT; 		// child LT first key */
								// ct occurrences of [key,rec,childGE] */
	bpt_key_t fkey; 			// first occurrence */
} bpt_node_t;

#define BPT_ROOT_SIZE_NUM_OF_SECTORS	3
// one node for each open handle */
typedef struct BPT_struct_t {
	struct EDB_struct_t* pm;

	int keySize;				// key length */
	bool dupKeys;				// true if duplicate keys */
	int sectorSize; 			// block size for idx records */
	bpt_comparison_t comp; 		// pointer to compare routine */

	bpt_node_t *root; 			// root of b-tree, room for 3 sets */
	bool rootModified;
	bool rootValid;

	void *malloc2; 				// malloc'd resources */
	bpt_node_t *gbuf; 			// gather buffer, room for 3 sets */

	unsigned int maxCt; 		// minimum # keys in node */
	int ks; 					// sizeof key entry */
	int keyId;
	uint32_t indexRootPage[BPT_ROOT_SIZE_NUM_OF_SECTORS];
	uint32_t operationNumber;
	struct {
		/* statistics */
		int maxHeight; /* maximum height attained */
		int nNodesIns; /* number of nodes inserted */
		int nNodesDel; /* number of nodes deleted */
		int nKeysIns; /* number of keys inserted */
		int nKeysDel; /* number of keys deleted */
	} stats;
} BPT_t;

#endif
#endif /* BPT_TYPES_H */
