/*
 * rawPageIOAccess.h
 *
 */

#ifndef EDB_PAGE_RAWPAGEIOACCESS_PRV_H_
#define EDB_PAGE_RAWPAGEIOACCESS_PRV_H_

#include <edb/config.h>
#if INCLUDE_EDB

#include "../pageMgr/pm_types.h"


EDB_result_t fpm_readRawPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf, uint32_t pgCt) ;
EDB_result_t fpm_writeRawPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf, uint32_t pgCt);
EDB_result_t FPM_QA_writeRawPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf) ;
EDB_result_t fpm_writeRawPages(EDB_Hndl_t pgMgrHndl, uint32_t num, pgIdx_t startPgIdx, void** bufs) ;


#endif
#endif /* EDB_PAGE_RAWPAGEIOACCESS_PRV_H_ */
