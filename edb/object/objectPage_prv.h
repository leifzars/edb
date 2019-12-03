/*
 * objectPage_prv.h
 *
 */

#ifndef EDB_OBJECT_OBJECT_PAGE_PRV_H
#define EDB_OBJECT_OBJECT_PAGE_PRV_H


#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm.h>
#include "objectPageHeader.h"

void fpm_objPg_append(FPM_objPgHeader_t* ownerPg, FPM_objBasePgHeader_t* prevPg, FPM_objBasePgHeader_t* newPg);

void fpm_objPg_setTail(FPM_objPgHeader_t* ownerPg, FPM_objBasePgHeader_t* nTail);
#endif


#endif /* EDB_OBJECT_OBJECT_PAGE_PRV_H */
