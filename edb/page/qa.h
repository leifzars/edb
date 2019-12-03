/*
 * qa.h
 *
 */

#ifndef EDB_PAGE_QA_H
#define EDB_PAGE_QA_H
#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_QA

#include "../edb_lib.h"

void fpm_qa_simplePageRWValidate(edb_pm_openArgsPtr_t openArgs);
void fpm_qa_rawPageSpeedTest(edb_pm_openArgsPtr_t openArgs);

void edb_pg_qa_outOfSpace(edb_pm_openArgsPtr_t openArgs);

#endif
#endif
#endif /* EDB_PAGE_QA_H */
