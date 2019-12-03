/*
 * pm_types.h
 *
 */

#ifndef EDB_PAGEMGR_PM_TYPES_H
#define EDB_PAGEMGR_PM_TYPES_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "stdio.h"

#include "os.h"
#include "lib/doubleLinkList.h"

#include "pm_s.h"

#include "../pageHeaders/fileHeader.h"

#include <edb/bTree/types.h>
#include "../page/pageAccessExtensions.h"

#include "../wal/walTypes.h"
#include "../transaction/transactionTypes.h"



typedef struct edb_pm_pageBuffer {
	//uint8_t buffer[OSL_PM_PAGE_SIZE];
	/* location of node */
	struct edb_pm_pageBuffer *next; /* next */
	struct edb_pm_pageBuffer *prev; /* previous */
	pgIdx_t pgIdx; /* on disk */
#if EDB_ENABLE_X
	uint32_t dataVersion;
	EDB_xHndl_t tranOwner;
#endif
#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
	uint32_t crc;
#endif
	void* p; /* in memory */
	uint8_t useCount;
	struct {
		uint8_t synched :1; /* true if buffer contents valid */
		//uint8_t synched :1;
		uint8_t modified :1; /* true if buffer modified */
	};
} edb_pm_pageBuf_t;

typedef edb_pm_pageBuf_t* edb_pm_pgBufHndl_t;


typedef EDB_result_t (*edb_pm_pageRead_ex)(void* owner, pgIdx_t pgIdx, void* buf);
typedef EDB_result_t (*edb_pm_pageWrite_ex)(void* owner, edb_pm_pgBufHndl_t pgBuf);
typedef EDB_result_t (*edb_pm_pagesWrite_ex)(void* owner, uint32_t num, edb_pm_pgBufHndl_t* bufs);

typedef struct {
	edb_pm_pageRead_ex readPg;
	edb_pm_pageWrite_ex writePg;
	edb_pm_pagesWrite_ex writePgs;
	bool enableWrite;
} edb_pm_pageAccessExtensions_t;



typedef struct fpm_pageCache {
	edb_pm_pageBuf_t bufList;
	uint32_t pageBufferCount;
	edb_pm_pageBuf_t* pageBufferList;
	void* pageBuffers;
} edb_pm_pageCache_t;

typedef uint32_t kvp_key_t;
typedef uint32_t pageIndex_t;

typedef struct EDB_struct_t {
	struct {
		FILE* fd_ptr;
		//struct BPT_struct_t vIndex;
		edb_pm_pageCache_t pgCache;

		pgIdx_t firstPageOffset;
		uint32_t fileSystemAllocationSize;

		char* fileName;
		union {
			FPM_fileHeader_t s;
			FPM_fileHeader_t fileHeader;
		};
		FPM_fileHeader_t* tmpFileHeaderBuffer;

		struct {
			uint64_t pageWriteCt;
			uint64_t pageFreeCt;
			uint64_t pageAllocCt;
		} stats;

		uint32_t isHeaderDirty;

	} fpm;

	void* owner;


	int lastIOError;

	doubleLinkList_t tables;

#if EDB_ENABLE_X
	edb_pm_pageAccessExtensions_t pgAcEx;

	edb_wal_t wal;

	EDB_xId_t xIdNext;
	EDB_xId_t dataVersion;

	struct {
		// active active reader, or writer if writer must == activeWriter
		EDB_xHndl_t active;

#if EDB_ENABLED_NESTED_X
		struct {
			EDB_xId_t dataVersion;
		} prev;

#if EDB_ENABLED_NESTED_WRITE_X
		// activeTopWriter: active X that is a writting X or that has a writting child
		EDB_xHndl_t activeTopWriter;
#endif
		// activeWriter: active X that is a writting X or that has a writting child
		EDB_xHndl_t activeWriter;

		doubleLinkList_t blockedWriters;
		doubleLinkList_t writersCommited;
		_OS_SEMAPHORE_STRUC wrtSem;

		doubleLinkList_t allActive;

#endif
		struct {
			EDB_x_t x;
			bool xUsed;
		} cache;

	} x;
#endif

	struct {
		uint32_t isNew :1;
		uint32_t isInBadSate :1;
		uint32_t isReadOnly :1;
		uint32_t needsRecovery :1;
		uint32_t isRecovering :1;
		uint32_t oppendWithCorruptHeader :1; // Not properly closed :1;
		uint32_t isIOBlockMode :1;
		uint32_t isAllocated :1;
#if EDB_ENABLE_X
		uint32_t isWalActive :1;
		uint32_t isWalSyncing :1;
		uint32_t isWalRecovering :1;
#endif

#if EDB_QA
		uint8_t qa_BlockHeaderUpdate :1;
#endif
	};


} EDB_t;

typedef EDB_t* EDB_Hndl_t;

#endif
#endif /* EDB_PAGEMGR_PM_TYPES_H */
