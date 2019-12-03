/*
 * qa.c
 *
 */
#include <edb/config.h>
#include "../test/test_base.h"

#if INCLUDE_EDB
#if EDB_ENABLE_X
#if EDB_QA_X

#include "../edb_lib.h"

typedef struct {
	clock_t sw;
	uint32_t notFoundCount;
	uint32_t deletedCount;
	uint32_t foundCount;
	uint32_t foundPrevious;
	uint32_t errCount;
	uint32_t getCount;
} getStats_t;

static uint32_t setTest(EDB_xHndl_t x, char* test, edb_tblHndl_t tbl, int startKey, int endKey);
static void getTest(EDB_xHndl_t x, char*test, edb_tblHndl_t tbl, int startKey, int endKey, getStats_t* stats,
bool postRecovery);

/*
 * Long writer with readers
 */
void EDB_QA_transactions1(edb_pm_openArgsPtr_t openArgs) {
	EDB_Hndl_t edbHndl = NULL;
	getStats_t getStats;
	edb_tblHndl_t tbl;
	int recordCount = 1000;
	EDB_xHndl_t x;
	EDB_xHndl_t x1;
	EDB_xHndl_t x2;
	uint32_t ct;

	EDB_PRINT_QA("Testing - Nested Transaction - Long Writer Many Child Readers");

	remove(openArgs->fileName);
	EDB_open(&edbHndl, openArgs);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	EDB_tableAdd(x, 1, &tbl);
	EDB_trns_commit(x);

	_EDB_BUSY();

	{
		EDB_trns_start(edbHndl, &x1, EDB_X_ANY);
		ct = setTest(x1, "Set ALL", tbl, 1, 10);
		_ASSERT(ct == 10);

		getTest(x1, "Get All R", tbl, 1, recordCount, &getStats, false);
		_ASSERT(getStats.foundCount == 10);

		{
			EDB_trns_start(edbHndl, &x2, EDB_X_ANY);
			getTest(x2, "Get All R", tbl, 1, recordCount, &getStats, false);
			_ASSERT(getStats.foundCount == 0);
			EDB_trns_commit(x2);
		}

		EDB_trns_commit(x1);

		{
			EDB_trns_start(edbHndl, &x2, EDB_X_ANY);
			getTest(x2, "Get All R", tbl, 1, recordCount, &getStats, false);
			_ASSERT(getStats.foundCount == 10);
			EDB_trns_commit(x2);
		}

	}

	_EDB_BUSY();

	EDB_close(edbHndl);
	edbHndl = NULL;
	EDB_PRINT_QA_PASS();
}

/*
 * Long running reader with short writer during
 */
void EDB_QA_transactions2(edb_pm_openArgsPtr_t openArgs) {
	EDB_Hndl_t edbHndl = NULL;
	getStats_t getStats;
	edb_tblHndl_t tbl;
	int recordCount = 1000;
	int wrtCt = 10;
	EDB_xHndl_t x;
	EDB_xHndl_t x1;
	EDB_xHndl_t x2;
	EDB_xHndl_t x3;
	uint32_t ct;

	EDB_PRINT_QA("Testing - Nested Transaction - Long Reader Child Writer");

	remove(openArgs->fileName);
	EDB_open(&edbHndl, openArgs);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	EDB_tableAdd(x, 1, &tbl);
	EDB_trns_commit(x);

	_EDB_BUSY();

	{
		//Simple add to start off
		EDB_trns_start(edbHndl, &x1, EDB_X_ANY);
		ct = setTest(x1, "Set ALL", tbl, 1, wrtCt);
		_ASSERT(ct == 10);

		getTest(x1, "Get All R", tbl, 1, recordCount, &getStats, false);
		_ASSERT(getStats.foundCount == 10);

		EDB_trns_commit(x1);
	}

	{
		//X1 will be long reader
		EDB_trns_start(edbHndl, &x1, EDB_X_ANY);

		{
			//X2 will be short writer
			EDB_trns_start(edbHndl, &x2, EDB_X_ANY);
			//Test X1
			ct = setTest(x2, "Set ALL", tbl, wrtCt + 1, wrtCt * 2);
			_ASSERT(ct == 10);

			getTest(x2, "Get All R", tbl, 1, recordCount, &getStats, false);
			_ASSERT(getStats.foundCount == wrtCt * 2);

			EDB_trns_commit(x2);
		}

		{
			//X3 prove X2 took
			EDB_trns_start(edbHndl, &x3, EDB_X_ANY);
			getTest(x3, "Get All R", tbl, 1, recordCount, &getStats, false);
			_ASSERT(getStats.foundCount == wrtCt * 2);
			EDB_trns_commit(x3);
		}

		//X1 test that is sees nothing since start
		getTest(x1, "Get All R", tbl, 1, recordCount, &getStats, false);
		_ASSERT(getStats.foundCount == wrtCt);
		EDB_trns_commit(x1);
	}

	_EDB_BUSY();

	EDB_close(edbHndl);
	EDB_PRINT_QA_PASS();
}

/*
 * Long running reader with many short writers during
 */
void EDB_QA_transactions3(edb_pm_openArgsPtr_t openArgs) {
	uint32_t writXCt = 10;
	uint32_t wrtCt = 10;
	uint32_t maxRecCt = wrtCt * writXCt;

	EDB_Hndl_t edbHndl = NULL;
	getStats_t getStats;
	edb_tblHndl_t tbl;
	EDB_xHndl_t x;
	EDB_xHndl_t x1;
	EDB_xHndl_t x2;
	EDB_xHndl_t x3;
	uint32_t ct;

	EDB_PRINT_QA("Testing - Nested Transaction - Long Reader Many Child Writer");

	remove(openArgs->fileName);
	EDB_open(&edbHndl, openArgs);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	EDB_tableAdd(x, 1, &tbl);
	EDB_trns_commit(x);

	_EDB_BUSY();

	{
		//X1 will be long reader
		EDB_trns_start(edbHndl, &x1, EDB_X_ANY);

		for (uint32_t i = 1; i <= writXCt; i++) {
			//X2 will be short writer
			EDB_trns_start(edbHndl, &x2, EDB_X_ANY);
			//Test X1
			ct = setTest(x2, "Set ALL", tbl, 1, wrtCt * i);
			_ASSERT(ct == wrtCt * i);

			getTest(x2, "Get All R", tbl, 1, maxRecCt, &getStats, false);
			_ASSERT(getStats.foundCount == wrtCt * i);

			EDB_trns_commit(x2);

			{
				//X3 prove X2 took
				EDB_trns_start(edbHndl, &x3, EDB_X_ANY);
				getTest(x3, "Get All R", tbl, 1, maxRecCt, &getStats, false);
				_ASSERT(getStats.foundCount == wrtCt * i);
				EDB_trns_commit(x3);
			}
		}

		//X1 test that is sees nothing since start
		getTest(x1, "Get All R", tbl, 1, maxRecCt, &getStats, false);
		_ASSERT(getStats.foundCount == 0);
		EDB_trns_commit(x1);

		{
			//Prove all iterations of writes took
			EDB_trns_start(edbHndl, &x3, EDB_X_ANY);
			getTest(x3, "Get All R", tbl, 1, maxRecCt, &getStats, false);
			_ASSERT(getStats.foundCount == wrtCt * writXCt);
			EDB_trns_commit(x3);
		}
	}

	_EDB_BUSY();

	EDB_close(edbHndl);
	EDB_PRINT_QA_PASS();
}

/*
 * Canceled Transaction
 */
void EDB_QA_transactions4(edb_pm_openArgsPtr_t openArgs) {
	uint32_t writXCt = 10;
	uint32_t wrtCt = 10;
	uint32_t maxRecCt = wrtCt * writXCt;

	EDB_Hndl_t edbHndl = NULL;
	getStats_t getStats;
	edb_tblHndl_t tbl;
	EDB_xHndl_t x;
	EDB_xHndl_t x1;
	EDB_xHndl_t x2;
	EDB_xHndl_t x3;
	uint32_t ct;

	EDB_PRINT_QA("Testing - Nested Transaction - Canceled Writer");

	remove(openArgs->fileName);
	EDB_open(&edbHndl, openArgs);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	EDB_tableAdd(x, 1, &tbl);
	EDB_trns_commit(x);

	_EDB_BUSY();

	{
		//Simple add to start off
		EDB_trns_start(edbHndl, &x1, EDB_X_ANY);
		ct = setTest(x1, "Set ALL", tbl, 1, wrtCt);
		_ASSERT(ct == 10);

		getTest(x1, "Get All R", tbl, 1, maxRecCt, &getStats, false);
		_ASSERT(getStats.foundCount == 10);

		EDB_trns_commit(x1);
	}

	{
		EDB_trns_start(edbHndl, &x1, EDB_X_ANY);

		getTest(x1, "Get All R", tbl, 1, maxRecCt, &getStats, false);
		_ASSERT(getStats.foundCount == 10);

		EDB_trns_cancel(x1);

		{
			EDB_trns_start(edbHndl, &x2, EDB_X_ANY);
			//Test X1
			ct = setTest(x2, "Set ALL", tbl, 1, wrtCt * 2);
			_ASSERT(ct == wrtCt * 2);

			getTest(x2, "Get All R", tbl, 1, maxRecCt, &getStats, false);
			_ASSERT(getStats.foundCount == wrtCt * 2);

			EDB_trns_cancel(x2);
		}

		{
			//Prove all iterations of writes took
			EDB_trns_start(edbHndl, &x3, EDB_X_ANY);
			getTest(x3, "Get All R", tbl, 1, maxRecCt, &getStats, false);
			_ASSERT(getStats.foundCount == wrtCt);
			EDB_trns_commit(x3);
		}
	}

	_EDB_BUSY();

	EDB_close(edbHndl);
	EDB_PRINT_QA_PASS();
}


void getTestX(char*test, edb_tblHndl_t tbl, int startKey, int endKey, getStats_t* stats, bool postRecovery) {
	EDB_xHndl_t x;

	stats->sw = clock();

	EDB_trns_start(tbl->edbHndl, &x, EDB_X_ANY);

	getTest(x, test, tbl, startKey, endKey, stats, postRecovery);

	EDB_trns_commit(x);
	stats->sw = clock() - stats->sw;
}


void getTest(EDB_xHndl_t x, char*test, edb_tblHndl_t tbl, int startKey, int endKey, getStats_t* stats,
bool postRecovery) {
	EDB_result_t opRc;
	buf_t vh1;
	buf_t vh2;
	uint8_t value1[VALUE_SIZE];

	int key = startKey;
	vh1.data = value1;
	vh1.dataLen = VALUE_SIZE;


	EDB_PRINT_QA("\t\tX%d> Get %s (%d->%d)", x->xId, test, startKey, endKey);
	memset(stats, 0, sizeof(getStats_t));

	while (1) {
		edb_qa_getValueBuffer(vh1.data, key, vh1.dataLen);

		opRc = EDB_getPage(x, tbl, key, &vh2);
		if (opRc == EDB_OKAY) {

			if (!edb_qa_valueTest(&vh1, &vh2, edb_qa_testIteration)) {
				if (postRecovery) {
					if (!edb_qa_valueTest(&vh1, &vh2, edb_qa_testIteration - 1)) {
						_ASSERT(0);
					} else {
						stats->foundPrevious++;
						stats->foundCount++;
					}
				} else {
					_ASSERT(0);
				}
			} else {
				stats->foundCount++;
			}

		} else if (opRc == EDB_NOT_FOUND) {
			stats->notFoundCount++;
		} else if (opRc == EDB_DELETED) {
			stats->deletedCount++;

		} else {
			stats->errCount++;
		}

		stats->getCount++;
		if (!edb_qa_stepKey(startKey, endKey, &key))
			break;
	}
	EDB_PRINT_QA("\t\t\t Found %d old %d nf:%d d:%d e:%d", stats->foundCount,stats->foundPrevious,stats->notFoundCount,stats->deletedCount,stats->errCount);
}


uint32_t setTest(EDB_xHndl_t x, char* test, edb_tblHndl_t tbl, int startKey, int endKey) {
	EDB_result_t opRc;
	buf_t vh1;
	uint8_t value1[VALUE_SIZE];
	int ct = 0;
	int key = startKey;

	vh1.data = value1;
	vh1.dataLen = VALUE_SIZE;

	while (1) {
		edb_qa_getValueBuffer(vh1.data, key, vh1.dataLen);
		opRc = EDB_set(x, tbl, key, &vh1);
		_ASSERT(opRc == EDB_OKAY);

		ct++;
		if (!edb_qa_stepKey(startKey, endKey, &key))
			break;
	}

	EDB_PRINT_QA("\t\tX%d> Set %s (%d->%d)", x->xId, test, startKey, endKey);

	return ct;
}

#endif
#endif
#endif
