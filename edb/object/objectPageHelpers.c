/*
 * objectPageHelpers.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "lib/assert.h"

#include <edb/pageMgr/pm_prv.h>

void fpm_objPg_append(FPM_objPgHeader_t* ownerPg, FPM_objBasePgHeader_t* prevPg,
		FPM_objBasePgHeader_t* newPg) {
	newPg->ownerPage = ownerPg->base.base.pageNumber;
	newPg->nextPage = 0;
	newPg->prevPage = prevPg->base.pageNumber;
	prevPg->nextPage = newPg->base.pageNumber;
	ownerPg->base.prevPage = newPg->base.pageNumber;
}

void fpm_objPg_setTail(FPM_objPgHeader_t* ownerPg, FPM_objBasePgHeader_t* nTail) {
	nTail->nextPage = 0;
	if (&ownerPg->base != nTail)
		ownerPg->base.prevPage = nTail->base.pageNumber;
	else
		ownerPg->base.prevPage = 0;
}

#endif
