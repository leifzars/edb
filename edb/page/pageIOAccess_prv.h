/*
 * pageAccess.h
 *
 */

#ifndef EDB_PAGE_PAGE_IO_ACCESS_PRV_H
#define EDB_PAGE_PAGE_IO_ACCESS_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm_types.h>

EDB_result_t fpm_readPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf);
EDB_result_t fpm_writePage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf);
EDB_result_t fpm_writePages(EDB_Hndl_t pgMgrHndl, uint32_t num, edb_pm_pgBufHndl_t* bufs_w_linearAddress_ACS);



//EDB_result_t fpm_readPartialPage(pgMgrHndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf, int32_t len);
//EDB_result_t fpm_writePartialPage(pgMgrHndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf, int32_t len);



#endif
#endif /* EDB_PAGE_PAGE_IO_ACCESS_PRV_H */
