/*
 * objectAccess_ext_qa.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "qa.h"
#include "lib/assert.h"

static void writeMemValue(void* bufPtr, uint32_t offset, buf_t* b) {
	memcpy(bufPtr + offset, b->data, b->dataLen);
}
static void wirteValue(FPM_Object_t* v, uint32_t offset, buf_t* b) {
	EDB_result_t err;
	err = FPM_objectScanToOffset(v, offset);
	_ASSERT(err == EDB_OKAY);
	err = EDB_obj_write(v, b);
	_ASSERT(err == EDB_OKAY);
}

//#if EDB_ENABLE_X
void fpm_qa_valueAccess_ext_test(edb_pm_openArgsPtr_t openArgs, bool testSpaceConstraints) {
#define WRITE_VALUE_BUF_SIZE	(1024 * 8)
#define VALUE_MAX_SIZE			(2048 * 8)
	EDB_Hndl_t fpmHndl = NULL;
	//FPM_t pgMgr;
	EDB_result_t err;
	pgIdx_t nPgIdx;
	buf_t b;
	FPM_Object_t v;
	uint32_t offset;
	void* compairBuffer;
	void* tmp;
	uint32_t space;

	b.data = _OS_MAlloc(WRITE_VALUE_BUF_SIZE);
	b.dataLen = WRITE_VALUE_BUF_SIZE;

	compairBuffer = _OS_MAlloc(VALUE_MAX_SIZE);
	tmp = _OS_MAlloc(VALUE_MAX_SIZE);

	EDB_PRINT_QA("Testing - Object standard operations space constrained: %d", testSpaceConstraints);

	if(openArgs->isFile)
		remove(openArgs->fileName);
//	edb_trns_commit(x3);
	openArgs->useWAL = false;
	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);

	if (testSpaceConstraints) {
		edb_pm_setMaxPageCount(fpmHndl, 50);
	}

	//Init value
	memset(b.data, 1, WRITE_VALUE_BUF_SIZE);
	b.dataLen = WRITE_VALUE_BUF_SIZE;
	err = FPM_insertValue(fpmHndl, 4, b.data, b.dataLen, &nPgIdx);
	_ASSERT(err == EDB_OKAY);
	if (testSpaceConstraints) {
		space = edb_pm_availablePages(fpmHndl);
		//_ASSERT(FPM_availablePages() == );
	}
	writeMemValue(compairBuffer, 0, &b);

	err = edb_obj_open(fpmHndl, &v, nPgIdx);
	_ASSERT(err == EDB_OKAY);

	FPM_objectScanToStart(&v);

	err = EDB_obj_scanToEnd(&v);
	_ASSERT(err == EDB_OKAY);

	//Scan to zero and update all
	EDB_PRINT_QA("\t\t - Scan to zero and update all");
	memset(b.data, 2, WRITE_VALUE_BUF_SIZE);
	offset = 0;
	wirteValue(&v, offset, &b);
	if (testSpaceConstraints) {
		space = edb_pm_availablePages(fpmHndl);
		//_ASSERT(FPM_availablePages() == );
	}
	writeMemValue(compairBuffer, offset, &b);

	//Test first page boundary
	EDB_PRINT_QA("\t\t - Test first page boundary");
	memset(b.data, 3, WRITE_VALUE_BUF_SIZE);
	b.dataLen = 10;
	offset = FPM_VALUE_PAGE_DATA_SIZE;
	wirteValue(&v, offset, &b);
	writeMemValue(compairBuffer, offset, &b);

	//Test second page boundary
	EDB_PRINT_QA("\t\t - Test second page boundary");
	memset(b.data, 4, WRITE_VALUE_BUF_SIZE);
	b.dataLen = 10;
	offset = FPM_VALUE_PAGE_DATA_SIZE + FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE;
	wirteValue(&v, offset, &b);
	writeMemValue(compairBuffer, offset, &b);

	//Test first page boundary again
	EDB_PRINT_QA("\t\t - Test first page boundary again");
	memset(b.data, 5, WRITE_VALUE_BUF_SIZE);
	b.dataLen = 10;
	offset = FPM_VALUE_PAGE_DATA_SIZE;
	wirteValue(&v, offset, &b);
	writeMemValue(compairBuffer, offset, &b);

	FPM_objectScanToStart(&v);
	err = EDB_obj_read(&v, tmp, WRITE_VALUE_BUF_SIZE);
	_ASSERT(err == EDB_OKAY);
	_ASSERT(memcmp(tmp, compairBuffer, WRITE_VALUE_BUF_SIZE) == 0);

	//Extend the value
	EDB_PRINT_QA("\t\t - Extend the value");
	memset(b.data, 6, WRITE_VALUE_BUF_SIZE);
	b.dataLen = WRITE_VALUE_BUF_SIZE;
	offset = FPM_objectLen(&v);
	wirteValue(&v, offset, &b);
	if (testSpaceConstraints) {
		space = edb_pm_availablePages(fpmHndl);
		//_ASSERT(FPM_availablePages() == );
	}
	writeMemValue(compairBuffer, offset, &b);

	EDB_obj_close(&v);
	edb_pm_close(fpmHndl);
	fpmHndl = NULL;
	//Re Open and Test value

	EDB_PRINT_QA("\t\t - Re Open and Test value");
	openArgs->resetAll = false;
	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);
	openArgs->resetAll = false;

	err = edb_obj_open(fpmHndl, &v, nPgIdx);
	_ASSERT(err == EDB_OKAY);

	err = EDB_obj_read(&v, tmp, VALUE_MAX_SIZE);
	_ASSERT(err == EDB_OKAY);

	_ASSERT(memcmp(tmp, compairBuffer, VALUE_MAX_SIZE) == 0);

	EDB_obj_close(&v);
	EDB_close(fpmHndl);
	_OS_Free(b.data);
	_OS_Free(compairBuffer);
	_OS_Free(tmp);

	EDB_PRINT_QA_PASS();
}

//#endif

#if !EDB_ENABLE_X
void edb_obj_qa_truncate(edb_pm_openArgsPtr_t openArgs) {
	EDB_result_t err;
	pgIdx_t nPgIdx;
	EDB_Hndl_t fpmHndl = NULL;
	FPM_Object_t v;
	buf_t data;
	char buffer[2048];

	EDB_PRINT_QA("Testing - Object truncate");

	if(openArgs->isFile)
		remove(openArgs->fileName);

	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);

	err = FPM_objectCreate(fpmHndl, &v, &nPgIdx);
	_ASSERT(err == EDB_OKAY);

	memset(buffer, 1, 2048);

	data.data = buffer;
	data.dataLen = 2048;

	err = EDB_obj_write(&v, &data);

	err = FPM_objectScanToOffset(&v, 512);
	EDB_obj_truncate(&v);

	_ASSERT(512 == FPM_objectLen(&v));

	memset(buffer, 2, 2048);

	err = FPM_objectScanToOffset(&v, 0);

	err = EDB_obj_read(&v, buffer, 513);
	_ASSERT(err == EDB_PM_EXCEEDS_EOF_VALUE);

	err = EDB_obj_read(&v, buffer, 512);
	_ASSERT(err == EDB_OKAY);

	err = FPM_objectScanToOffset(&v, 511);
	EDB_obj_truncate(&v);

	err = FPM_objectScanToOffset(&v, 1);
	EDB_obj_truncate(&v);

	err = FPM_objectScanToOffset(&v, 0);
	EDB_obj_truncate(&v);

	EDB_obj_close(&v);
	EDB_close(fpmHndl);

	EDB_PRINT_QA_PASS();
}
#endif

#endif
