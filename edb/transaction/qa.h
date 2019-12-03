/*
 * qa.h
 *
 */

#ifndef EDB_TRANSACTION_QA_H
#define EDB_TRANSACTION_QA_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X
#if EDB_QA_X

 void EDB_QA_transactions1(edb_pm_openArgsPtr_t openArgs);
 void EDB_QA_transactions2(edb_pm_openArgsPtr_t openArgs) ;
 void EDB_QA_transactions3(edb_pm_openArgsPtr_t openArgs);
 void EDB_QA_transactions4(edb_pm_openArgsPtr_t openArgs);

#endif
#endif
#endif
#endif /* EDB_TRANSACTION_QA_H */
