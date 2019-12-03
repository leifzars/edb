/*
 * test_base.h
 *
 */

#ifndef EDB_TEST_TEST_BASE_H
#define EDB_TEST_TEST_BASE_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "string.h"
#include "stdio.h"
#include "time.h"

#include "lib/assert.h"

#define VALUE_SIZE	450//(FPM_VALUE_PAGE_DATA_SIZE)
#define PRINT_STATS		false


extern uint32_t edb_qa_testIteration;

bool edb_qa_stepKey(int startKey, int endKey, int* key);
void edb_qa_getValueBuffer(void* buf, uint32_t key, uint32_t len);
bool edb_qa_valueTest(buf_t* vh1, buf_t* vh2, uint32_t iteration) ;

#endif
#endif /* EDB_TEST_TEST_BASE_H */
