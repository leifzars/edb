/*
 * wal_types.h
 *
 */

#ifndef EDB_WAL_WALTYPES_H
#define EDB_WAL_WALTYPES_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X
//#include "../pageHeaders/pageHeader.h"

#include "walIndexTypes.h"

typedef uint32_t FPM_chkPntId_t;

typedef struct {
	//pgIdx_t walRegionStart;
	//pgIdx_t walRegionEnd;
	uint32_t startPgIdx;

	uint32_t size;
	uint32_t sizePgCt;

	//uint32_t writePos;

	//void* activeAppendPage;
	void* appendBuffer;

	uint32_t activeWritePgIdx; //Relative To file
	uint32_t activeWritePgPos; //Relative To page
	uint32_t activeWritePos; //Relative To wall

	void* readBuffer;
	uint32_t readBufferPgIdx; //Relative To file


	uint32_t appedBufferDirty: 1; //IsDirty
	uint32_t appedBufferValid: 1; //IsDirty
	uint32_t readBufferValid: 1; //IsDirty

	uint32_t cumulativeCRC;

	uint32_t currentWALIdx; //

#if EDB_WAL_INDEX_VIA_HASH
	kh_pgLookUp_t* indexLookUp;
#else
	vec_t index;
#endif

	uint32_t salt;

} edb_wal_t;

typedef struct {
	pgIdx_t walRegionStart;
	pgIdx_t walRegionEnd;

	uint32_t currentCheckPoint; //
} FPM_WAL_status_t;

typedef enum {
	FPM_WAL_UNKNOWN,
	FPM_WAL_HEADER,
	FPM_WAL_X_START,
	FPM_WAL_X_COMMIT,
	FPM_WAL_X_CANCEL,
	FPM_WAL_PAGE,
	FPM_WAL_CHECK_POINT,
} FPM_WAL_entry_e;

typedef struct {
	FPM_WAL_entry_e type;
	//uint8_t pad[3];
	uint32_t salt;

	union {
		struct {
			FPM_chkPntId_t next;
		} header;
		struct {
			uint32_t xId;
		} x;
		struct {
			uint32_t xId;
			pgIdx_t pgIdx;
		} page;
		struct {
			FPM_chkPntId_t next;
		} chkPnt;

		uint32_t args[2];
	};
	uint32_t crc;
} FPM_WAL_entry_t;

#endif
#endif
#endif /* EDB_WAL_WALTYPES_H */
