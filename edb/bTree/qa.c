/*
 * qa.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_QA
#include "time.h"

#include "qa.h"
#include "lib/assert.h"



/*
 * expected to run after edb_bpt_qa_general
 */
void edb_bpt_qa_duplicateKeys(edb_pm_openArgsPtr_t openArgs, uint32_t keyCount, uint32_t dupCount) {
	EDB_Hndl_t fpmHndl = NULL;
	EDB_result_t err;
	pgIdx_t indexRootsDup[BPT_ROOT_SIZE_NUM_OF_SECTORS] = { 0, 0, 0 };
	BPT_t idxInstDup;
	BPT_Result_e bptErr;
	_ASSERT(dupCount >= 1 && keyCount >= 1);

	EDB_PRINT_QA("Testing - B+Tree duplicate keys: %d duplicateCount: %d.... ", keyCount, dupCount);

	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);

	bptErr = BPT_open_UInt32(&idxInstDup, fpmHndl, indexRootsDup, true);
	uint32_t j, k, val;
	for (uint32_t i = 0; i < keyCount * dupCount; i++) {
		j = i % keyCount;
		bptErr = BPT_insertKey(&idxInstDup, &j, i);
		_ASSERT(bptErr == BPT_OKAY);
	}

	BPT_cursor_t crs;
	BPT_record_t r;
	for (uint32_t i = 0; i < keyCount; i++) {
		j = i;

		bptErr = BPT_initSearch_Equal(&idxInstDup, &crs, j);
		_ASSERT(bptErr == BPT_OKAY);

		while (BPT_findNext(&crs, &r) == BPT_CS_CURSOR_ACTIVE) {
			_ASSERT(r.valuePgIdx == j);
			j += keyCount;
		}
	}

	uint32_t dupToDelete = dupCount / 2;
	for (uint32_t i = 0; i < keyCount; i++) {
		val = i + dupToDelete * keyCount;
		bptErr = BPT_deleteKey(&idxInstDup, &i, &val);
		_ASSERT(bptErr == BPT_OKAY);
	}

	for (uint32_t i = 0; i < keyCount; i++) {
		val = i + dupToDelete * (keyCount / dupCount);

		bptErr = BPT_initSearch_Equal(&idxInstDup, &crs, i);
		_ASSERT(bptErr == BPT_OKAY);
		k = 0;
		while (BPT_findNext(&crs, &r) == BPT_CS_CURSOR_ACTIVE) {
			if (k == dupToDelete)
				k++;
			_ASSERT(r.valuePgIdx == i + k * keyCount);
			k++;
		}
	}

	BPT_close(&idxInstDup);
	EDB_close(fpmHndl);

	EDB_PRINT_QA_PASS();
}

void edb_bpt_qa_iterateReverse(edb_pm_openArgsPtr_t openArgs, uint32_t keyCount, uint32_t dupCount) {
	pgIdx_t indexRootsDup[BPT_ROOT_SIZE_NUM_OF_SECTORS] = { 0, 0, 0 };
	BPT_t idxInstDup;
	BPT_Result_e bptErr;
	EDB_t* pgMgr;
	EDB_result_t err;
	_ASSERT(dupCount >= 1 && keyCount >= 1);
	pgMgr = NULL;

	EDB_PRINT_QA("Testing - B+Tree iterate keys: %d duplicateCount: %d.... ", keyCount, dupCount);

	if(openArgs->isFile)
		remove(openArgs->fileName);

	err = EDB_open(&pgMgr, openArgs);
	_ASSERT(err == EDB_OKAY);

	bptErr = BPT_open_UInt32(&idxInstDup, pgMgr, indexRootsDup, dupCount > 1);
	uint32_t j, k;
	for (uint32_t i = 0; i < keyCount * dupCount; i++) {
		j = i % keyCount;
		bptErr = BPT_insertKey(&idxInstDup, &j, i);
		_ASSERT(bptErr == BPT_OKAY);
	}

	BPT_cursor_t crs;
	BPT_record_t r;
	uint32_t ct;
	uint32_t dupCt;
	for (uint32_t i = 0; i < keyCount; i++) {
		j = k = i;
		ct = 0;
		dupCt = dupCount;

		bptErr = BPT_initSearch_Range(&idxInstDup, &crs, 0, k, BPT_PT_DESC);
		_ASSERT(bptErr == BPT_OKAY);

		while (BPT_findNext(&crs, &r) == BPT_CS_CURSOR_ACTIVE) {
			_ASSERT(r.valuePgIdx == keyCount * (dupCt - 1) + j);
			if (--dupCt == 0) {
				dupCt = dupCount;
				j--;
			}

			ct++;
		}
		_ASSERT(ct == (1 + i) * dupCount);
	}

	BPT_close(&idxInstDup);
	EDB_close(pgMgr);

	EDB_PRINT_QA_PASS();
}

/*
 *
 */
void edb_bpt_qa_general(edb_pm_openArgsPtr_t openArgs, bool testSpaceConstraints) {
#define KEY_COUNT	(5000 * EDB_QA_TEST_ITERATION_FACTOR)
	EDB_Hndl_t fpmHndl = NULL;
	EDB_result_t err;
	BPT_Result_e bptErr;
	bpt_cursor_t idx;
	uint32_t failedAtIdx = 0;
	uint32_t tmpMaxPageCount;

	clock_t sw;
	float avgKeyInsertTime;
	float avgKeyLookupTime;
	uint32_t space;

	EDB_PRINT_QA("Testing - B+Tree basics space constrained: %d.... ", testSpaceConstraints);

	if(openArgs->isFile)
		remove(openArgs->fileName);


	tmpMaxPageCount = openArgs->maxPageCount;
	if (testSpaceConstraints)
		openArgs->maxPageCount = 10;

	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);

	pgIdx_t indexRoots[BPT_ROOT_SIZE_NUM_OF_SECTORS] = { 0, 0, 0 };
	BPT_t idxInst;

	openArgs->maxPageCount = tmpMaxPageCount;
//	if (testSpaceConstraints) {
//		FPM_setMaxPageCount(fpmHndl, 10);
//	}

	bptErr = BPT_open_UInt32(&idxInst, fpmHndl, indexRoots, false);
	_ASSERT(bptErr == BPT_OKAY);

	sw = clock();
	for (uint32_t i = 0; i < KEY_COUNT; i++) {
		bptErr = BPT_insertKey(&idxInst, &i, i * 100);
		if (testSpaceConstraints) {
			space = edb_pm_availablePages(fpmHndl);
			if (bptErr && space == 0) {
				_ASSERT((EDB_result_t)bptErr == EDB_PM_IO_ERROR_FULL);
				failedAtIdx = i;
				break;
			}
		}
		_ASSERT(bptErr == BPT_OKAY);
	}
	sw = clock() - sw;
	avgKeyInsertTime = sw / (float) KEY_COUNT;

	sw = clock();
	for (uint32_t i = 0; i < KEY_COUNT; i++) {
		bptErr = BPT_findKey(&idxInst, &i, &idx);
		if (failedAtIdx > 0 && failedAtIdx == i) {
			_ASSERT(bptErr == BPT_NOT_FOUND);
			break;
		}
		_ASSERT(bptErr == BPT_OKAY);
		_ASSERT(i * 100 == idx.valueOffset);
	}
	sw = clock() - sw;
	avgKeyLookupTime = sw / (float) KEY_COUNT;

	BPT_close(&idxInst);

	EDB_PRINT_QA("BPT access speed, insert: %f ms, lookup: %f ms", (double)avgKeyInsertTime, (double)avgKeyLookupTime);

	EDB_close(fpmHndl);

	EDB_PRINT_QA_PASS();

}

#endif
#endif



