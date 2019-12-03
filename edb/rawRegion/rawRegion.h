/*
 * rawRegion.h
 *
 */

#ifndef EDB_RAWREGION_RAWREGION_H_
#define EDB_RAWREGION_RAWREGION_H_


#include <edb/config.h>

#if INCLUDE_EDB

typedef struct{

	struct{
		void* ptr;
		uint32_t pgIdx;
		bool valid;
		uint32_t size;
		//uint32_t pgCt;
	}buf;

	uint32_t pgIdx;
	uint32_t offset;

}FPM_RR_region_t, *FPM_RR_regionHndl_t;

EDB_result_t FPM_RR_getNewRegion(EDB_Hndl_t pgMgrHndl, uint32_t pgCnt, uint32_t* out_pageIdx);

#endif
#endif /* EDB_RAWREGION_RAWREGION_H_ */
