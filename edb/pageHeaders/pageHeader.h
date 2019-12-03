/*
 * pageHeader.h
 *
 */

#ifndef EDB_PAGEHEADERS_PAGEHEADER_H
#define EDB_PAGEHEADERS_PAGEHEADER_H

#include <edb/config.h>
#if INCLUDE_EDB

typedef uint32_t pgIdx_t;


typedef enum
	FORCE_ENUM_SIZE
	{
		EDB_PM_GROUP_TYPE_FREE,
	EDB_PM_GROUP_TYPE_INDEX,
	EDB_PM_GROUP_TYPE_VALUE,
	EDB_PM_GROUP_TYPE_VALUE_HISTORY,
	__EDB_PM_GROUP_TYPE_SIZE
} edb_pm_pageGroupType_e;

typedef enum
	FORCE_ENUM_SIZE
	{
		EDB_PM_PT_UNKNOWN, EDB_PM_PT_HEADER, EDB_PM_PT_EMPTY_ALLOCATED, // only on first use of page, from file expansion
	EDB_PM_PT_FREE,
	EDB_PM_PT_RAW,
	EDB_PM_PT_INDEX,

	EDB_PM_PT_OBJ_RAW,
	EDB_PM_PT_OBJ_RAW_OF,

	EDB_PM_PT_TABLE_DEF,
	EDB_PM_PT_TABLE_DEF_OVERFLOW,

	EDB_PM_PT_INDEX_DEF,
	EDB_PM_PT_INDEX_DEF_OVERFLOW,

	EDB_PM_PT_VALUE,
	EDB_PM_PT_VALUE_OVERFLOW,

} pageType_e;


#define EDB_PM_PAGE_DATA_SIZE	(_OS_getPageSize() - sizeof(edb_pm_pageHeader_t))

typedef PRE_PACKED struct {
	uint32_t crc;
	pageType_e type;
	uint8_t pad[3];
	uint32_t pageNumber;
	uint64_t opperationId;
}POST_PACKED edb_pm_pageHeader_t;


#endif
#endif /* EDB_PAGEHEADERS_PAGEHEADER_H */
