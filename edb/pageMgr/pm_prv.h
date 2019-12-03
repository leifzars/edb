/*
 * pm_prv.h
 *
 */

#ifndef EDB_PAGEMGR_PM_PRV_H
#define EDB_PAGEMGR_PM_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "lib/assert.h"

#include <edb/pageMgr/pm.h>

#include "../pageHeaders/pageTypeDescriptor.h"

#include "../rawRegion/rawRegion.h"

#include "../page/pageCache_prv.h"
#include "../page/rawPageIOAccess_prv.h"
#include "../page/pageAccess_prv.h"
#include "../object/objectPage_prv.h"
#include "../object/objectAccess_ext_prv.h"
#include "../value/valueAccess_prv.h"

#include "../table/table_prv.h"
#include "../index/index_prv.h"
#include "../transaction/transaction_prv.h"
#include "../wal/wal.h"
#include "../error.h"

#define FPM_HEADER_PG_IDX		0

EDB_result_t edb_getFreePage(EDB_Hndl_t pgMgrHndl, pgIdx_t* pageIdx);

EDB_result_t edb_freePage(EDB_Hndl_t pgMgrHndl, pgIdx_t pageIdx) ;

uint32_t edb_getPageCount(EDB_Hndl_t pgMgrHndl);

pgIdx_t edb_getResourcePgIdx(EDB_Hndl_t pgMgrHndl, edb_pm_resource_e type);
void edb_setResourcePgIdx(EDB_Hndl_t pgMgrHndl, edb_pm_resource_e type, pgIdx_t pgIdx);


#if EDB_ENABLE_X
EDB_result_t edb_preOp(EDB_xHndl_t x);
void edb_postOp(EDB_xHndl_t x);
#endif



#endif
#endif /* EDB_PAGEMGR_PM_PRV_H */
