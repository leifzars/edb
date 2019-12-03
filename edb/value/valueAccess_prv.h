/*
 * valueAccess_prv.h
 *
 */

#ifndef EDB_VALUE_VALUEACCESS_PRV_H
#define EDB_VALUE_VALUEACCESS_PRV_H


#include <edb/config.h>
#if INCLUDE_EDB

#include "../pageMgr/pm_types.h"
#include "../pageHeaders/valuePageHeader.h"


EDB_result_t fpm_freeValue(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx);
uint32_t fpm_calcRequieredAdditionalPages(uint32_t objLen, uint32_t offset, uint32_t appendLen);
#endif
#endif /* EDB_VALUE_VALUEACCESS_PRV_H */
