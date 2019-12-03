/*
 * walIndexTypes.h
 *
 */

#ifndef EDB_WAL_WALINDEXTYPES_H
#define EDB_WAL_WALINDEXTYPES_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "lib/vector.h"

#if EDB_WAL_INDEX_VIA_HASH
#include "lib/kHash.h"

__KHASH_TYPE(pgLookUp, uint32_t, uint32_t)
#else
#include "lib/vector.h"
#endif

//
//typedef struct {
//	uint32_t xId;
//	vecOfPgStatus_t modifiedPages;
//} EDB_WAL_xStatus_t;


typedef struct {
	pgIdx_t pgIdx;
	uint32_t walOffset;
} EDB_WAL_pgStatus_t;


typedef vec_t vecOfPgStatus_t;

#endif
#endif
#endif /* EDB_WAL_WALINDEXTYPES_H */
