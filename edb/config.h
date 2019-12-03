/*
 * config.h
 *
 */

#ifndef EDB_CONFIG_H
#define EDB_CONFIG_H

#include "../env.h"

#define INCLUDE_EDB 1

#if INCLUDE_EDB


//******************** Parameters **************************

#define EDB_IS_CONSTRAINED 			false		//Embedded Set True

/*
 * Helpers based on page size
 */
#define EDB_PAGE_MASK				0x1FF
#define EDB_PAGE_BIT_POSS			9


/*
 * EDB_PAGE_CACHE
 * Page cache size
 * Min tested is 16
 */
#if EDB_IS_CONSTRAINED
#define EDB_PAGE_CACHE				16
#else
#define EDB_PAGE_CACHE				128
#endif

/*
 * EDB_WAL_MAX_PG_CT
 * The WAL is pre allocated and fixed in size
 * No transaction should consume more than 1/2 of the WAL
 */
#if EDB_IS_CONSTRAINED
#define EDB_WAL_MAX_PG_CT			(1024*2)
#else
#define EDB_WAL_MAX_PG_CT			(1024*8)
#endif

/*
 * EDB_WAL_AUTO_SYNC_PG_CT
 * Auto sync WAL at transaction end if the WAL has written
 * more then this number of pages.
 * Recommended (EDB_WAL_MAX_PG_CT / 2)
 */
#define EDB_WAL_AUTO_SYNC_PG_CT			(EDB_WAL_MAX_PG_CT / 2)

/*
 * EDB_WAL_AUTO_SYNC_WAL_INDEX_CT
 * Auto sync WAL at transaction end, if the WAL has indexed
 * more then this number of pages. The WAL likely has multiple
 * versions of the same page, indexed only once. *
 * Recommended (EDB_WAL_AUTO_SYNC_WAL_INDEX_CT / 2)
 */
#define EDB_WAL_AUTO_SYNC_WAL_INDEX_CT	(EDB_WAL_MAX_PG_CT / 2)

/*
 * EDB_X_AVG_PAGE_CNT
 * Preallocate transaction size for X number of pages
 */
#define EDB_X_AVG_PAGE_CNT				16

//******************** Features **************************
/*
 * EDB_ENABLE_X
 * Transaction Support
 * Only disable this for testing low level function
 */
#define EDB_ENABLE_X				1

/*
 * EDB_ENABLED_NESTED_X
 * Multiple Readers Single Writer
 */
#define EDB_ENABLED_NESTED_X		1

/*
 * EDB_ENABLED_NESTED_WRITE_X
 * Multiple Readers & Writers
 * Requires semaphores search for macro OS_SEMAPHORE
 */
#define EDB_ENABLED_NESTED_WRITE_X	1

/*
 * Sorts cache into continues pages before write.
 * Requires additional buffers/memory (max additional = EDB_PAGE_CACHE).
 * malloc failure is handled gracefully with out allocation
 * and without optimization.
 */
#define EDB_ENABLE_CACHE_ADV_WRITE		1



//******************** Testing **************************
#define EDB_QA							0
#if EDB_QA
#define EDB_TEST_BROKEN					0

#define EDB_QA_TEST_ITERATION_FACTOR	100
/*
 * Low leve tests.
 * Disable transaction support to run.
 * Set EDB_ENABLE_X to 0
 */
#define EDB_TEST_PAGE_OPS				0
#define EDB_TEST_BPT_OPS				0
#define EDB_TEST_OBJ_OPS				0


/*
 * Higher function tests
 *
 */
#define EDB_QA_X						1
#define EDB_QA_WAL						1
#define EDB_QA_RUN_CRASH_TEST			1

#if EDB_QA_X
#undef EDB_ENABLE_X_DIAG
#define EDB_ENABLE_X_DIAG 				0
#endif

#endif

//******************** Diagnostics **************************
#define EDB_INCLUDE_DIRTY_PAGE_CHECK			0
#define EDB_INCLUDE_PAGE_DIAG					0

#define EDB_INCLUDE_WAL_DIAG					0
#define EDB_INCLUDE_EXT_WAL_DIAG				0
#define EDB_ENABLE_X_DIAG						0

#include "config_errors.h"

#endif

#endif /* EDB_CONFIG_H */
