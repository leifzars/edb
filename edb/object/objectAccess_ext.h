/*
 * valueAccess_ext.h
 *
 */

#ifndef EDB_OBJECT_OBJECT_ACCESS_EXT_H
#define EDB_OBJECT_OBJECT_ACCESS_EXT_H
#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm_prv.h>

typedef struct {
	uint32_t offset;
	void* ptr; //Pointer to cached block
	uint32_t len;
	uint32_t pgOffset;
} FPM_objectCursor_t;

typedef struct {
	EDB_Hndl_t pgMgrHndl;
#if EDB_ENABLE_X
	EDB_xHndl_t xHndl;
#endif
	void* oph;
	void* activePage;
	uint32_t activePageValueOffset; // value offset of start of activePage value chunk

	uint16_t objPgHeaderSize;
	uint16_t objOfPgHeaderSize;

	uint16_t objPgDataSpace;
	uint16_t objOfPgDataSpace;

	FPM_objectCursor_t cursor;
} FPM_Object_t;

#if EDB_ENABLE_X
EDB_result_t FPM_objectOpen(EDB_xHndl_t xHndl, FPM_Object_t* valHndl, pgIdx_t valuePgIdx);
EDB_result_t FPM_objectCreate(EDB_xHndl_t xHndl, FPM_Object_t* valHndl, pgIdx_t* out_valuePgIdx);

#else
EDB_result_t FPM_objectOpen(EDB_Hndl_t edb, FPM_Object_t* valHndl, pgIdx_t valuePgIdx);
EDB_result_t FPM_objectCreate(EDB_Hndl_t edb, FPM_Object_t* valHndl, pgIdx_t* out_valuePgIdx);
#endif

EDB_result_t edb_obj_open(EDB_Hndl_t edbHndl, FPM_Object_t* valHndl, pgIdx_t valuePgIdx);
void EDB_obj_close(FPM_Object_t* valHndl);
void edb_obj_close(FPM_Object_t* valHndl);

void FPM_objectScanToStart(FPM_Object_t* valHndl);
EDB_result_t EDB_obj_scanToEnd(FPM_Object_t* valHndl);
EDB_result_t edb_obj_scanToEnd(FPM_Object_t* valHndl);

EDB_result_t FPM_objectScanToOffset(FPM_Object_t* valHndl, uint32_t offset);

EDB_result_t EDB_obj_write(FPM_Object_t* valHndl, buf_t* buf);
EDB_result_t edb_obj_write(FPM_Object_t* valHndl, buf_t* buf);

EDB_result_t EDB_obj_read(FPM_Object_t* valHndl, void* buf, uint32_t len);
EDB_result_t edb_obj_read(FPM_Object_t* valHndl, void* buf, uint32_t len);
EDB_result_t EDB_obj_readR(FPM_Object_t* valHndl, void* buf, uint32_t len);
EDB_result_t edb_obj_readR(FPM_Object_t* valHndl, void* buf, uint32_t len);

EDB_result_t EDB_obj_truncate(FPM_Object_t* valHndl);
EDB_result_t edb_obj_truncate(FPM_Object_t* valHndl);

uint32_t FPM_objectLen(FPM_Object_t* valHndl);
uint32_t FPM_objectPageIndex(FPM_Object_t* valHndl);

typedef void (*FPM_objCallBack_t)(EDB_Hndl_t pgMgrHndl, void* page, void* arg);

#if EDB_ENABLE_X
EDB_result_t EDB_obj_add(EDB_xHndl_t xHndl, pageType_e pgType, pgIdx_t* out_valuePgIdx, pgIdx_t ownerPgIdx,
		FPM_objCallBack_t cb, void* arg);
#endif
EDB_result_t edb_obj_add(EDB_Hndl_t pgMgrHndl, pageType_e pgType, pgIdx_t* out_valuePgIdx, pgIdx_t ownerPgIdx,
		FPM_objCallBack_t cb, void* arg);


#endif
#endif /* EDB_OBJECT_OBJECT_ACCESS_EXT_H */
