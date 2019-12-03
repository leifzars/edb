/*
 * FPM_fileHeader.h
 *
 */

#ifndef EDB_PAGEHEADERS_FILEHEADER_H
#define EDB_PAGEHEADERS_FILEHEADER_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "time.h"
#include "pageHeader.h"

typedef enum
	FORCE_ENUM_SIZE
	{
		FPM_V_UNKNOWN, FPM_V_1,
} FPM_version_e;

typedef enum
	FORCE_ENUM_SIZE
	{
		FPM_R_UNKNOWN, FPM_R_TABLE_DEFS, FPM_R_INDEX_DEFS, FPM_R_WAL,
} edb_pm_resource_e;

typedef PRE_PACKED struct {
	edb_pm_resource_e type;
	pgIdx_t pagIdx;
}FPM_resourceLocation_t;
#define FPM_MAX_RESOURCE_COUNT		16

#define FPM_FILE_MAGIC_NUMBER	(0xAE57BA0D)

typedef PRE_PACKED struct {
	edb_pm_pageHeader_t base;
	FPM_version_e version;
	uint32_t magicNumber;
	time_t createTime;

	struct {
		uint32_t isOpen :1; // Not properly closed
		uint32_t isDirty :1; // Not properly closed
		uint32_t isCorrupt :1; // Not properly closed
		//uint32_t isInTransaction :1; // Not properly closed
	};


	pgIdx_t usedPageCount;

	uint32_t freePageCount;
	pgIdx_t nextFreePage;

	uint32_t maxPageCount;

	uint32_t walSyncIndx;


	FPM_resourceLocation_t resources[FPM_MAX_RESOURCE_COUNT];

}POST_PACKED FPM_fileHeader_t;

#endif
#endif /* EDB_PAGEHEADERS_FILEHEADER_H */
