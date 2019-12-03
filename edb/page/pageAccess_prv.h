/*
 * pageAccess_prv.h
 *
 */

#ifndef EDB_PAGE_PAGE_ACCESS_PRV_H
#define EDB_PAGE_PAGE_ACCESS_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB

//private may only contain public includes
#include <edb/pageMgr/pm.h>
#include "pageAccess.h"

EDB_result_t fpm_flushBuffer(EDB_Hndl_t pgMgrHndl, edb_pm_pageBuf_t* buf);
EDB_result_t fpm_clearBuffer(EDB_Hndl_t pgMgrHndl, edb_pm_pageBuf_t* buf);

#endif
#endif /* EDB_PAGE_PAGE_ACCESS_PRV_H */
