/*
 * test.c
 *
 */

#include "config.h"
#if INCLUDE_EDB

#include "stdlib.h"

#include "test/test_base.h"

#include "edb_lib.h"

#include "page/qa.h"
#include "bTree/qa.h"
#include "object/qa.h"
#include "transaction/qa.h"


#if PRINT_STATS
#define FPM_PRINT_STATUS(edb)	FPM_printStatus(edb)
#else
#define FPM_PRINT_STATUS(edb)
#endif

#if EDB_INCLUDE_PAGE_DIAG
	#define FPM_ASSERT_PAGE_CACHE_UNUSED(edb)	fpm_assertPageCacheUnused(edb)
#else
	#define FPM_ASSERT_PAGE_CACHE_UNUSED(edb)
#endif

typedef struct {
	clock_t sw;
	uint32_t notFoundCount;
	uint32_t deletedCount;
	uint32_t foundCount;
	uint32_t foundPrevious;
	uint32_t errCount;
	uint32_t getCount;
} getStats_t;


static char testFile[] = "a:edbTest.db";

static edb_pm_openArgs_t edbTest_openArgs = { false,	//resetAll
		false,	//resetOnCorruption
		true,	//isFile
		testFile, //fileName
		0, //firstPageOffset
		0, //maxPageCount
		true, //useWAL
		};

static edb_pm_openArgs_t fpmTest_openArgs_Raw = { true,	//resetAll
		false,	//resetOnCorruption
		false,	//isFile
		NULL, //fileName
		0, //firstPageOffset
		1024 * 256, //maxPageCount
		true, //useWAL
		};

#if EDB_ENABLE_X
static void testSetX(char*test, edb_tblHndl_t tbl, int startKey, int endKey);
static void testAppendX(char* test, edb_tblHndl_t tbl, int startKey, int endKey);
static void testGetX(char*test, edb_tblHndl_t tbl, int startKey, int endKey, getStats_t* stats, bool postRecovery);

static void getTest_AfterRecovery(char*test, edb_tblHndl_t tbl, int startKey, int endKey);
static void testIterateAll(char*test, edb_tblHndl_t tbl);
static void testIterateRange(char*test, edb_tblHndl_t tbl, uint32_t keyLow, uint32_t keyHigh);
static void testDeleteX(char*test, edb_tblHndl_t tbl, int startKey, int endKey, int mod);

static void showGetStats(char*test, edb_tblHndl_t tbl, getStats_t* stats);

#if EDB_ENABLE_X
static uint32_t setTest(EDB_xHndl_t x, char* test, edb_tblHndl_t tbl, int startKey, int endKey);
static void getTest(EDB_xHndl_t x, char*test, edb_tblHndl_t tbl, int startKey, int endKey, getStats_t* stats,
bool postRecovery);
#endif

#if EDB_QA_RUN_CRASH_TEST
///////////////////////////////////////////////
//---------Recover after random Reboot
//////////////////////////////////////////////
uint32_t recoveryTestTimerElapsed(vssTimer_t UNUSED(timer)) {
	FS_powerOffSDCard();
	SYS_RL_Restart(BOOT_SW_RESET_ASSERT_FAULT);
	return 0;
}

void EDB_QA_testRecovery(uint32_t iteration, uint32_t recordCount) {
	EDB_result_t err;
	FPM_Hndl_t edbHndl;
	edb_tblHndl_t tbl;
	getStats_t getStats;

	EDB_open(&edbHndl, &edbTest_openArgs);
	_ASSERT(edbHndl != NULL);
	err = EDB_tableGet(edbHndl, 1, &tbl);

	edb_qa_testIteration = iteration;
	testGetX("Get All R", &tbl, 1, recordCount, &getStats, true);

	EDB_close(edbHndl);
}

void EDB_QA_simulateRndPwrLossDuringAccess(uint32_t iterations, uint32_t recordCount) {
	EDB_result_t err;
	FPM_Hndl_t edbHndl;
	edb_tblHndl_t tbl;
	bool isFirstGetAfterRecovery = true;

	remove(testFile);
	EDB_PRINT_I("Starting RecoveryTest");

	do {
		EDB_open(&edbHndl, &edbTest_openArgs);
		_ASSERT(edbHndl != NULL);
		err = EDB_tableGet(edbHndl, 1, &tbl);

		edb_qa_testIteration++;
		EDB_PRINT_I("Simulation iteration");

		testSetX("Set ALL", &tbl, 1, recordCount);

		EDB_close(edbHndl);

	} while (edb_qa_testIteration < iterations);
}

void EDB_QA_simulateRndPwrLossDuringAccessTest(FPM_openArgsPtr_t openArgs) {
#define ITERATIONS	500
	EDB_result_t err;
	FPM_Hndl_t edbHndl;
	edb_tblHndl_t tbl;
	getStats_t getStats;
	int recordCount = 100;
	vssTimer_t tmr = NULL;
	EDB_xHndl_t x;
	uint32_t delay_ms;
	RNGA_getRandomData(&delay_ms);
	delay_ms = delay_ms % (20 * 1000);

	bool isFirstGetAfterRecovery = true;

	edb_qa_testIteration = SREG_DATA_PTR->uInts[0];

	if (edb_qa_testIteration >= ITERATIONS) {
		return;
	} else if (edb_qa_testIteration == 0) {
		remove(testFile);
		openArgs->resetAll = true;
		EDB_PRINT_I("Starting RecoveryTest");
	} else {
		openArgs->resetAll = false;
		EDB_PRINT_I("Resuming RecoveryTest, itr %d", edb_qa_testIteration);
		isFirstGetAfterRecovery = true;
	}
	do {
		err = EDB_open(&edbHndl, openArgs);
		openArgs->resetAll = false;
		_ASSERT(edbHndl != NULL);

		{
			EDB_trns_start(edbHndl, &x, EDB_X_ANY);
			if (edb_qa_testIteration == 0)
				err = EDB_tableAdd(x, 1, &tbl);
			else
				err = EDB_tableGet(x, 1, &tbl);
			_ASSERT(!err);

			if (edb_qa_testIteration) {
				getTest(x, "Get All R", &tbl, 1, recordCount, &getStats, isFirstGetAfterRecovery);
				isFirstGetAfterRecovery = false;
			}

			edb_qa_testIteration++;
			SREG_DATA_PTR->uInts[0] = edb_qa_testIteration;

			setTest(x, "Set ALL", &tbl, 1, recordCount);

			if (tmr == NULL) {
				tmr = VSST_getTimer(recoveryTestTimerElapsed);
				VSST_start(tmr, delay_ms, false);
			}

			EDB_trns_commit(x);
		}

		EDB_close(edbHndl);
		edbHndl = NULL;
		if (edb_qa_testIteration == ITERATIONS)
			break;
	} while (1);

	edb_qa_testIteration = 0;
}
//-------------------------------------------

//-------------------------------------------
#endif


#if !EDB_TEST_BROKEN
void EDB_QA_test2() {
	EDB_result_t err;
	EDB_Hndl_t edbHndl = NULL;
	edb_tblHndl_t tbl;
	getStats_t getStats;
	int recordCount = 10;
	EDB_xHndl_t x;

	remove(testFile);
	err = EDB_open(&edbHndl, &edbTest_openArgs);
	_ASSERT(edbHndl != NULL);
	_ASSERT(err == EDB_OKAY);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	err = EDB_tableAdd(x, 1, &tbl);
	EDB_trns_commit(x);

	_ASSERT(err == EDB_OKAY);

	for (edb_qa_testIteration = 0; edb_qa_testIteration < 6; edb_qa_testIteration++) {
		testSetX("Set ALL", tbl, 1, recordCount);

		testGetX("Get All R", tbl, recordCount, 1, &getStats, true);

		//testIterateAll("Iterate All", tbl);

		testDeleteX("Delete Mod 2", tbl, 1, recordCount, 2);

		//testIterateAll("Iterate All", tbl);

		testGetX("Get All", tbl, 1, recordCount, &getStats, false);

		testGetX("Get All", tbl, recordCount - 10, recordCount + 10, &getStats, false);


	}

	edb_qa_testIteration--;
	//edb_recover(edbHndl);

	testIterateAll("Iterate All", tbl);
	EDB_close(edbHndl);

}
#endif

void EDB_QA_testAppendX(edb_pm_openArgsPtr_t openArgs) {
	EDB_Hndl_t edbHndl = NULL;
	edb_tblHndl_t tbl;
	int recordCount = 100;
	EDB_xHndl_t x;

	EDB_PRINT_QA("Testing - Append");

	remove(testFile);
	EDB_open(&edbHndl, openArgs);
	_ASSERT(edbHndl != NULL);


	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	EDB_tableAdd(x, 1, &tbl);
	EDB_trns_commit(x);

	testSetX("Set ALL", tbl, 1, recordCount);

	testAppendX("Append ALL", tbl, 1, recordCount);

	EDB_close(edbHndl);

	EDB_PRINT_QA_PASS();
}

static void EDB_QA_commonOperations(edb_pm_openArgsPtr_t openArgs) {
	EDB_Hndl_t edbHndl = NULL;
	getStats_t getStats;
	EDB_result_t err;
	edb_tblHndl_t tbl;
	int recordCount = 1000;
	EDB_xHndl_t x;
	//EDB_QA_test2();

	EDB_PRINT_QA("Testing - Common Ops");

	remove(testFile);
	EDB_open(&edbHndl, openArgs);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	_ASSERT(edbHndl != NULL);
	err = EDB_tableAdd(x, 1, &tbl);
	_ASSERT(!err);
	EDB_trns_commit(x);

	_EDB_BUSY();

	testSetX("Set ALL", tbl, 1, recordCount);

	testGetX("Get All R", tbl, recordCount, 1, &getStats, false);
	showGetStats("Get All R", tbl, &getStats);

	_EDB_BUSY();
	//showGetStats(edbHndl, &getStats);

	testSetX("Set All R", tbl, recordCount, 1);

	testGetX("Get All R", tbl, recordCount, 1, &getStats, false);
	showGetStats("Get All R", tbl, &getStats);

	_EDB_BUSY();

	testIterateAll("Iterate All", tbl);

	testDeleteX("Delete Mod 2 R", tbl, recordCount, 1, 2);

	testIterateAll("Iterate All", tbl);

	testIterateRange("Iterate 100-200", tbl, 100, 200);

	testDeleteX("Delete Mod 10", tbl, 1, recordCount, 10);

	testIterateAll("Iterate All", tbl);

	_EDB_BUSY();

	testDeleteX("Delete Mod 3", tbl, 1, recordCount, 3);

	testIterateAll("Iterate All", tbl);

	testDeleteX("Delete All", tbl, 1, recordCount, 0);

	testIterateAll("Iterate All", tbl);

	testSetX("Set ALL", tbl, 1, recordCount);

	testGetX("Get All R", tbl, recordCount, 1, &getStats, false);

	EDB_close(edbHndl);
	edbHndl = NULL;
	//tbl = NULL;

	openArgs->resetAll = false;
	EDB_open(&edbHndl, openArgs);
	openArgs->resetAll = true;

	_ASSERT(edbHndl != NULL);

	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	err = EDB_tableGet(x, 1, &tbl);
	EDB_trns_commit(x);

	_EDB_BUSY();

	testGetX("Get All R", tbl, recordCount, 1, &getStats, false);
	showGetStats("Get All R", tbl, &getStats);

	EDB_close(edbHndl);
}

#endif
void EDB_test() {
	srand(time(0));
	//fpmTest_openArgs_Raw.fileName = "test.db";//FS_getPartitionName(FS_PARTITION_RAW);
	edbTest_openArgs.fileName = "test.db";
	remove(edbTest_openArgs.fileName);

#if EDB_TEST_PAGE_OPS
	fpm_qa_simplePageRWValidate(&edbTest_openArgs);
	fpm_qa_rawPageSpeedTest(&edbTest_openArgs);
	edb_pg_qa_outOfSpace(&edbTest_openArgs);
#endif

#if EDB_TEST_BPT_OPS
	 edb_bpt_qa_general(&edbTest_openArgs, false);
	 edb_bpt_qa_duplicateKeys(&edbTest_openArgs, 1000 * EDB_QA_TEST_ITERATION_FACTOR, 9);
	 edb_bpt_qa_general(&edbTest_openArgs, true);
	 edb_bpt_qa_iterateReverse(&edbTest_openArgs, 500, 1) ;
#endif

#if EDB_QA_X
	EDB_QA_commonOperations(&edbTest_openArgs);
	EDB_QA_testAppendX(&edbTest_openArgs);
	EDB_QA_test2(&edbTest_openArgs);
#endif

#if EDB_TEST_OBJ_OPS
	edb_obj_qa_truncate(&edbTest_openArgs);
	fpm_qa_valueAccess_ext_test(&edbTest_openArgs, false);
	fpm_qa_valueAccess_ext_test(&edbTest_openArgs, true);
#endif


#if EDB_QA_X && EDB_ENABLED_NESTED_X && EDB_ENABLED_NESTED_WRITE_X
	EDB_QA_transactions1(&edbTest_openArgs);
	EDB_QA_transactions2(&edbTest_openArgs);
	EDB_QA_transactions3(&edbTest_openArgs);
	EDB_QA_transactions4(&edbTest_openArgs);
#endif

#if EDB_QA_RUN_CRASH_TEST
	EDB_QA_simulateRndPwrLossDuringAccessTest(&edbTest_openArgs);
#endif

	return;

}

#if EDB_ENABLE_X

#define RAND_VALUE_BUFFER_MAX_LEN	2048
static void getRandValueBuffer(buf_t* buf, uint32_t key, uint32_t len) {
	uint32_t* ptr;
	uint32_t rnd;
	uint32_t appendNumber;

	if (len == 0) {
		rnd = rand();

		appendNumber = rnd % RAND_VALUE_BUFFER_MAX_LEN;
		if (appendNumber < 12)
			appendNumber = 12;
	} else {
		appendNumber = len;
	}

	buf->dataLen = appendNumber;
	ptr = (uint32_t*) buf->data;
	memset(buf->data, appendNumber, appendNumber);

	ptr[0] = edb_qa_testIteration;
	ptr[1] = key;
	ptr[2] = appendNumber;
}

#if EDB_ENABLE_X
void testAppendX(char* test, edb_tblHndl_t tbl, int startKey, int endKey) {
	EDB_result_t opRc;
	buf_t vh1;
	uint8_t value1[RAND_VALUE_BUFFER_MAX_LEN];

	clock_t sw;
	int ct = 0;
	int valueLenSum = 0;
	int key = startKey;

	vh1.data = value1;

	sw = clock();
	EDB_xHndl_t x;

	while (1) {
		EDB_trns_start(tbl->edbHndl, &x, EDB_X_ANY);
		for (int m = 0; m < 20; m++) {
			getRandValueBuffer(&vh1, key, 1024);
			opRc = EDB_append(x, tbl, key, &vh1);
			_ASSERT(opRc == EDB_OKAY);

			valueLenSum += vh1.dataLen;
		}
		EDB_trns_commit(x);
		ct++;
		if (!edb_qa_stepKey(startKey, endKey, &key))
			break;
	}

	sw = clock() - sw;
//printf("Insert, records: %d, elapsed time %d,\t avg time %dms ms", recordCount, sw.milliSeconds,  (float)recordCount / (float)sw.milliSeconds);
	float avg = sw / (float) ct;
	EDB_PRINT_I("\t\t%s, records: %d, valueLenSum: %d, time %lums, avg %f ms", test, ct, valueLenSum, sw,
			(double)avg);

	FPM_PRINT_STATUS(tbl->edbHndl);

	FPM_ASSERT_PAGE_CACHE_UNUSED(tbl->edbHndl);
}
#endif


#if EDB_ENABLE_X
void testSetX(char* test, edb_tblHndl_t tbl, int startKey, int endKey) {
	clock_t sw;
	int ct = 0;
	EDB_xHndl_t x;

	sw = clock();

	EDB_trns_start(tbl->edbHndl, &x, EDB_X_ANY);

	ct = setTest(x, test, tbl, startKey, endKey);

	EDB_trns_commit(x);

	sw = clock() - sw;
	float avg = sw / (float) ct;
	EDB_PRINT_QA("\t\t%s, records: %d, time %lu, avg %f ms", test, ct, sw, (double)avg);

	FPM_PRINT_STATUS(tbl->edbHndl);
	FPM_ASSERT_PAGE_CACHE_UNUSED(tbl->edbHndl);
}
#endif

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

	return ct;

}

#if EDB_QA_RUN_CRASH_TEST
void getTest_AfterRecovery(char*test, edb_tblHndl_t tbl, int startKey, int endKey) {
	EDB_result_t err;
	buf_t vh1;
	buf_t vh2;
	uint8_t value1[VALUE_SIZE];
	clock_t sw;
	int ct = 0;
	int key = startKey;
	int previousVersion = 0;
	vh1.data = value1;
	vh1.dataLen = VALUE_SIZE;

	sw = clock();
	EDB_xHndl_t x;
	EDB_trns_start(tbl->edbHndl, &x, EDB_X_ANY);
	while (1) {
		edb_qa_getValueBuffer(vh1.data, key, vh1.dataLen);
		uint32_t* ptr = (uint32_t*) vh1.data;

		err = EDB_getPage(x, tbl, key, &vh2);
		_ASSERT(err == EDB_OKAY);

		if (vh1.dataLen == vh2.dataLen) {
			if (edb_qa_testIteration) {

				ptr[0] = edb_qa_testIteration - 1;
				if (memcmp(vh1.data, vh2.data, vh1.dataLen) == 0) {
					//The normal execution
				} else {
					ptr[0] = edb_qa_testIteration;
					if (memcmp(vh1.data, vh2.data, vh1.dataLen) == 0) {
						//Simulated crash after set before edb_qa_testIteration++
						previousVersion++;
					} else {
						ptr[0] = edb_qa_testIteration - 2;
						if (memcmp(vh1.data, vh2.data, vh1.dataLen) == 0) {
							//Simulated crash after edb_qa_testIteration++ but before set
							previousVersion++;
						} else {
							_ASSERT(0);
						}
					}
				}

			}
		}

//	EDB_returnPage(edbHndl, vh2.data);

		if (!edb_qa_stepKey(startKey, endKey, &key))
			break;
		ct++;
	}
	EDB_trns_commit(x);
	sw = clock() - sw;
	EDB_PRINT_I("\t\t%s, records: %d, time %lu, avg %f ms", test, ct, sw, (double)(sw / (float )ct));
	if (previousVersion)
		EDB_PRINT_I("!!Previous Version: %d", previousVersion);

	FPM_PRINT_STATUS(tbl->edbHndl);

	FPM_ASSERT_PAGE_CACHE_UNUSED(tbl->edbHndl);
}

#endif


#if EDB_ENABLE_X
void testGetX(char*test, edb_tblHndl_t tbl, int startKey, int endKey, getStats_t* stats, bool postRecovery) {
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

}

void testIterateAll(char*test, edb_tblHndl_t tbl) {
	buf_t vh1;
	uint8_t value1[VALUE_SIZE];
	clock_t sw;
	uint32_t* ptr;
	BPT_cursor_t cur;
	BPT_record_t record;
	EDB_valueAccessInfo_t vai;
	EDB_result_t err;
	BPT_Result_e bRslt;
	int ct = 0;

	vh1.data = value1;
	vh1.dataLen = VALUE_SIZE;

	sw = clock();
	EDB_xHndl_t trns;
	EDB_trns_start(tbl->edbHndl, &trns, EDB_X_ANY);

	bRslt = EDB_idx_initSearch_All(trns, tbl->rowIndex, &cur, BPT_PT_ACS);
	_ASSERT(!bRslt);

	while (EDB_idx_findNext(trns, &cur, &record) == BPT_CS_CURSOR_ACTIVE) {
		vai.key = record.key;
		vai.pgIdx = record.valuePgIdx;

		edb_trns_enter(trns);
		err = FPM_getValuePage(tbl->edbHndl, &vai);
		_ASSERT(err == EDB_OKAY);

		edb_qa_getValueBuffer(vh1.data, vai.key, vh1.dataLen);
		if(!(vh1.dataLen == vai.valueSize && memcmp(vh1.data, vai.valueBuffer, vh1.dataLen) == 0)){
			ptr = (uint32_t*) vh1.data;
			EDB_PRINT_QA("%d %d %d",ptr[0],ptr[1],ptr[2]);

			ptr = (uint32_t*) vai.valueBuffer;
			EDB_PRINT_QA("%d %d %d",ptr[0],ptr[1],ptr[2]);
			_ASSERT(0);
		}

		fpm_returnPage(tbl->edbHndl, vai.pageBuffer, FPM_PGBUF_NONE);
		edb_trns_exit(trns);
		ct++;
	}
	EDB_trns_commit(trns);
	sw = clock() - sw;
	EDB_PRINT_QA("\t\t%s, records: %d, time %lums, avg %f ms", test, ct, sw, (double)(sw / (float )ct));

	FPM_PRINT_STATUS(tbl->edbHndl);

	FPM_ASSERT_PAGE_CACHE_UNUSED(tbl->edbHndl);
}

void testIterateRange(char*test, edb_tblHndl_t tbl, uint32_t keyLow, uint32_t keyHigh) {
	buf_t vh1;
	uint8_t value1[VALUE_SIZE];
	clock_t sw;
	BPT_cursor_t cur;
	BPT_record_t record;
	EDB_valueAccessInfo_t vai;
	BPT_Result_e bRslt;
	int ct = 0;

	vh1.data = value1;
	vh1.dataLen = VALUE_SIZE;

	sw = clock();
	EDB_xHndl_t trns;
	EDB_trns_start(tbl->edbHndl, &trns, EDB_X_ANY);

	bRslt = EDB_idx_initSearch_Range(trns, tbl->rowIndex, &cur, keyLow, keyHigh, BPT_PT_ACS);
	_ASSERT(!bRslt);

	while (EDB_idx_findNext(trns, &cur, &record) == BPT_CS_CURSOR_ACTIVE) {
		vai.key = record.key;
		vai.pgIdx = record.valuePgIdx;

		edb_trns_enter(trns);
		FPM_getValuePage(tbl->edbHndl, &vai);

		edb_qa_getValueBuffer(vh1.data, vai.key, vh1.dataLen);
		_ASSERT(vh1.dataLen == vai.valueSize && memcmp(vh1.data, vai.valueBuffer, vh1.dataLen) == 0);

		fpm_returnPage(tbl->edbHndl, vai.pageBuffer, FPM_PGBUF_NONE);

		edb_trns_exit(trns);
		ct++;
	}
	EDB_trns_commit(trns);
	sw = clock() - sw;
	EDB_PRINT_I("\t\t%s, records: %d, time %lums, avg %f ms", test, ct, sw, (double)(sw / (float )ct));

	FPM_PRINT_STATUS(tbl->edbHndl);
	FPM_ASSERT_PAGE_CACHE_UNUSED(tbl->edbHndl);
}



void testDeleteX(char*test, edb_tblHndl_t tbl, int startKey, int endKey, int mod) {
	EDB_result_t opRc;
	clock_t sw;
	int ct;
	int key = startKey;

	ct = 0;

	sw = clock();
	EDB_xHndl_t x;
	EDB_trns_start(tbl->edbHndl, &x, EDB_X_ANY);
	while (1) {
		if (mod == 0 || key % mod == 0) {
			opRc = EDB_delete(x, tbl, key);
			_ASSERT(!opRc || opRc == EDB_NOT_FOUND);

			ct++;
		}

		if (!edb_qa_stepKey(startKey, endKey, &key))
			break;
	}
	EDB_trns_commit(x);
	sw = clock() - sw;
	EDB_PRINT_I("\t\t%s, records: %d, time %lums, avg %f ms", test, ct, sw, (double)((double)sw / (double )ct));

	FPM_PRINT_STATUS(tbl->edbHndl);

	FPM_ASSERT_PAGE_CACHE_UNUSED(tbl->edbHndl);

}
#endif
void showGetStats(char*test, edb_tblHndl_t tbl, getStats_t* stats) {
	EDB_PRINT_I("\t\t%s, records: %d, time %fms, avg %f ms", test, stats->getCount, (double)stats->sw,
			(double)(stats->sw / (float )stats->getCount));
	EDB_PRINT_I("\t\t%s, f: %d, d: %d, nf: %d, e: %d", test, stats->foundCount, stats->deletedCount,
			stats->notFoundCount, stats->errCount);
	if (stats->foundPrevious) {
		EDB_PRINT_I("\t\t%s, records: %d, fp: %d", test, stats->foundCount, stats->foundPrevious);
	}
}


#endif
#endif
