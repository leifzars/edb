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

void fpm_qa_simplePageRWValidate(edb_pm_openArgsPtr_t openArgs) {
	struct pgBuf_t {
		edb_pm_pageHeader_t header;
		uint8_t data[496];
	};

	EDB_Hndl_t fpmHndl;
	EDB_result_t err;
	pgIdx_t nPgIdx;

	struct pgBuf_t* pgBuf;

	fpmHndl = NULL;

	EDB_PRINT_QA("Testing - low level page write & read .... ");
	//remove(testFile);
	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);

	for (int i = 1; i < 1001; i++) {
		err = fpm_getEmptyPage(fpmHndl, EDB_PM_PT_RAW, &nPgIdx, (void*) &pgBuf);
		_ASSERT(err == EDB_OKAY);

		memset(pgBuf->data, i, FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE);

		fpm_returnPage(fpmHndl, (void*) pgBuf, FPM_PGBUF_DIRTY);
		_ASSERT(err == EDB_OKAY);
	}

	for (int i = 1001; i-- > 1;) {
		err = FPM_getPage(fpmHndl, i, true, (void*) &pgBuf);
		_ASSERT(err == EDB_OKAY);

		for (int j = 0; j < FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE; j++) {
			_ASSERT(pgBuf->data[j] == (uint8_t )i);
		}

		fpm_returnPage(fpmHndl, (void*) pgBuf, FPM_PGBUF_DIRTY);
		_ASSERT(err == EDB_OKAY);
	}

	EDB_close(fpmHndl);
	EDB_PRINT_QA_PASS();
}


/*
 *
 * 512 page size
 * 	r: ~ 2.5 - 3.5 ms Or 200kb/s
 *  w: ~ 5.6 - 7.5 ms Or 90 kb/s
 *
 * 1024 page size
 * 	r: ~ 4.5 ms Or 220kb/s
 *  w: ~ 9.7 ms Or 103 kb/s
 *
 *  6/8/2014
 *  Os
 *  250 page size, 1000 pages
 *  r: ~ 4.6 ms
 *  w: ~ 4.1 ms
 *
 *  11/2/2017
 *  (2 11:08:07.008) [I]  [XX      ]        Raw page access speed pm:2, r: 1.474915 ms, w: 1.210815 ms
 *  (2 11:08:10.945) [I]  [XX      ]        Raw page access speed a:fpmTest.ps, r: 1.746368 ms, w: 2.149170 ms
 */

void fpm_qa_rawPageSpeedTest(edb_pm_openArgsPtr_t openArgs) {
#define NUM_OF_PAGES	(1000 * EDB_QA_TEST_ITERATION_FACTOR)
#define PAGE_BUF_SIZE	250
	EDB_Hndl_t fpmHndl;
	EDB_result_t err;
	uint8_t buff[PAGE_BUF_SIZE];
	edb_pm_pageHeader_t* ph;
	void* pd;
	clock_t sw;
	double avgWriteTime_ms;
	double avgReadTime_ms;
	fpmHndl = NULL;

	if(openArgs->isFile)
		remove(openArgs->fileName);

	EDB_PRINT_QA("Testing - low level page r/w speed .... ");

	err = EDB_open(&fpmHndl, openArgs);

	sw = clock();
	for (int i = 0; i < NUM_OF_PAGES; i++) {
		err = FPM_getPage(fpmHndl, i, false, (void**) &ph);
		_ASSERT(!err);
		ph->type = EDB_PM_PT_RAW;
		ph->pageNumber = i;
		pd = (void*) ((uint8_t*) ph + sizeof(edb_pm_pageHeader_t));
		memset(pd, i, PAGE_BUF_SIZE);
		fpm_returnPage(fpmHndl, (void*) ph, FPM_PGBUF_DIRTY);
	}
	sw = clock() - sw;
	sw = sw * _CLOCKS_PER_SEC_ / 1000;
	avgWriteTime_ms = sw / (float) NUM_OF_PAGES;

	sw = clock();
	for (int i = 0; i < NUM_OF_PAGES; i++) {
		err = FPM_getPage(fpmHndl, i, true, (void**) &ph);
		_ASSERT(!err);
		ph->type = EDB_PM_PT_RAW;
		pd = (void*) ((uint8_t*) ph + sizeof(edb_pm_pageHeader_t));

		memset(buff, i, PAGE_BUF_SIZE);
		_ASSERT(memcmp(buff, pd, PAGE_BUF_SIZE) == 0);

		fpm_returnPage(fpmHndl, (void*) ph, FPM_PGBUF_NONE);
	}

	EDB_close(fpmHndl);

	sw = clock() - sw;
	sw = sw * _CLOCKS_PER_SEC_ / 1000;
	avgReadTime_ms = sw  / (float) NUM_OF_PAGES;

	EDB_PRINT_QA("Raw page access speed (pg ct: %d) r: %f ms, w: %f ms",  NUM_OF_PAGES, avgReadTime_ms,
			avgWriteTime_ms);
	EDB_PRINT_QA_PASS();

}




 void edb_pg_qa_outOfSpace(edb_pm_openArgsPtr_t openArgs) {

	EDB_Hndl_t fpmHndl = NULL;
	EDB_result_t err;
	pgIdx_t nPgIdx;
	uint32_t maxPgCount = 20;
	uint32_t fpmPageOverhead = 1;
	struct pgBuf_t* pgBuf;
	uint32_t tmpMaxPageCount;

	EDB_PRINT_QA("Testing - out of space .... ");

	if(openArgs->isFile)
		remove(openArgs->fileName);

	tmpMaxPageCount = openArgs->maxPageCount;
	openArgs->maxPageCount = maxPgCount;

	err = EDB_open(&fpmHndl, openArgs);
	_ASSERT(err == EDB_OKAY);
	openArgs->maxPageCount = tmpMaxPageCount;


	for (uint32_t i = 1; i < maxPgCount + 5; i++) {
		err = fpm_getEmptyPage(fpmHndl, EDB_PM_PT_RAW, &nPgIdx, (void*) &pgBuf);
		if (i > (maxPgCount - fpmPageOverhead)) {
			_ASSERT(err == EDB_PM_IO_ERROR_FULL);
			break;
		} else {
			_ASSERT(err == EDB_OKAY);
		}

		fpm_returnPage(fpmHndl, (void*) pgBuf, FPM_PGBUF_DIRTY);

		_ASSERT(edb_pm_availablePages(fpmHndl) == (maxPgCount - fpmPageOverhead - i));
	}

	edb_freePage(fpmHndl, maxPgCount / 2);
	_ASSERT(edb_pm_availablePages(fpmHndl) == 1);

	EDB_close(fpmHndl);

	EDB_PRINT_QA_PASS();
//	uint32_t FPM_availablePages(FPM_Hndl_t pgMgrHndl) {
}

#endif
#endif
