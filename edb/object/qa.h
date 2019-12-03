/*
 * objectAccess_ext_qa.h
 *
 */

#ifndef EDB_OBJECT_QA_H
#define EDB_OBJECT_QA_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "../edb_lib.h"

void fpm_qa_valueAccess_ext_test(edb_pm_openArgsPtr_t openArgs, bool testSpaceConstraints);
void edb_obj_qa_truncate(edb_pm_openArgsPtr_t openArgs);
#endif
#endif /* EDB_OBJECT_QA_H */
