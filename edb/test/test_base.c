/*
 * test_base.c
 *
 */

#include "test_base.h"

uint32_t edb_qa_testIteration;

bool edb_qa_stepKey(int startKey, int endKey, int* key) {
	if (*key == endKey)
		return false;
	if (startKey > endKey) {
		(*key)--;
	} else {
		(*key)++;
	}
	return true;
}

void edb_qa_getValueBuffer(void* buf, uint32_t key, uint32_t len) {
	uint32_t* ptr = (uint32_t*) buf;
	memset(buf, key, len);

	ptr[0] = edb_qa_testIteration;
	ptr[1] = key;
}

bool edb_qa_valueTest(buf_t* vh1, buf_t* vh2, uint32_t iteration) {
	uint32_t* ptr;
	if (vh1->dataLen == vh2->dataLen) {

		ptr = (uint32_t*) vh1->data;
		ptr[0] = iteration;

		if (memcmp(vh1->data, vh2->data, vh1->dataLen) == 0) {
			return true;
		}

	}

	ptr = (uint32_t*) vh1->data;
	EDB_PRINT_QA("%d %d %d",ptr[0],ptr[1],ptr[2]);

	ptr = (uint32_t*) vh2->data;
	EDB_PRINT_QA("%d %d %d",ptr[0],ptr[1],ptr[2]);
	return false;
}
