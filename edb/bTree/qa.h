/*
 * qa.h
 *
 */

#ifndef BPT_QA_H
#define BPT_QA_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "../edb_lib.h"

void edb_bpt_qa_general(edb_pm_openArgsPtr_t openArgs, bool testSpaceConstraints);
void edb_bpt_qa_duplicateKeys(edb_pm_openArgsPtr_t openArgs, uint32_t keyCount, uint32_t dupCount);
void edb_bpt_qa_iterateReverse(edb_pm_openArgsPtr_t openArgs, uint32_t keyCount, uint32_t dupCount) ;

#endif
#endif /* BPT_QA_H */
