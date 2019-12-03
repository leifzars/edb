/*
 * trasactionTypes.h
 *
 */

#ifndef EDB_TRANSACTION_TYPES_H
#define EDB_TRANSACTION_TYPES_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "../wal/walIndexTypes.h"
#include "lib/doubleLinkList.h"

typedef uint32_t EDB_xId_t;


typedef enum {
	EDB_X_R, EDB_X_READ
} EDB_xType_e;

typedef enum {
	EDB_X_STATE_NOT_VALID, EDB_X_STATE_ACTIVE, EDB_X_STATE_COMMITTING, EDB_X_STATE_COMMITED, EDB_X_STATE_CANCELED,
} EDB_xState_e;

typedef struct EDB_x{
	dllElement_t n;
	struct EDB_struct_t* edbHndl;
	EDB_xId_t xId;
	EDB_xId_t dataVersion;

	EDB_xState_e state;
	//uint32_t requiresExclusiveWriteLock :1;
	uint32_t isWriter :1; //Is missing BPT state :(
	uint32_t isAutoX :1;
	uint32_t isFailed :1;
	uint32_t isReadOnly :1;

#if EDB_ENABLED_NESTED_WRITE_X
	struct EDB_x* parent;
	struct EDB_x* child;
#endif

	vecOfPgStatus_t pageRefs;
} EDB_x_t;
typedef EDB_x_t* EDB_xHndl_t;

#endif
#endif
#endif /* EDB_TRANSACTION_TYPES_H */
