/*
 * bpt_cursor.h
 *
 */

#ifndef BPT_CURSOR_H
#define BPT_CURSOR_H


#include <edb/config.h>
#if INCLUDE_EDB

#include "bpt.h"


typedef enum {
	BPT_CS_INVALID_INDEX = -1,
	BPT_CS_INVALID_CURSOR,
	BPT_CS_END_OF_RESULTS,
	BPT_CS_END_OF_RESULTS_ON_ERR,
	BPT_CS_CURSOR_INITIALIZED,
	BPT_CS_CURSOR_UNINITIALIZED,
	BPT_CS_CURSOR_ACTIVE,
	BPT_CS_STORE_CHANGED,

} BPT_cursorStatus_t;
typedef enum {
	BPT_PT_ACS, //ascending
	BPT_PT_DESC //Descending
} BPT_order_t;

typedef enum {
	BPT_PT_ALL,
	BPT_PT_EQUAL,
	BPT_PT_RANGE, //ascending
} BPT_predicateType_t;

typedef struct {
	union {
			struct {
				kvp_key_t lower;
				kvp_key_t upper;
			} range;
			kvp_key_t  equality_value;

		};

	BPT_predicateType_t type;
	BPT_order_t order;

} BPT_predicate_t;

typedef struct {
	BPT_hndl_t bpt;
	BPT_predicate_t predicate;
	uint32_t operationNumberOnStart;

	BPT_cursorStatus_t status; /**< Supertype of cursor		*/

	bpt_cursor_t bptCursor;
} BPT_cursor_t;
typedef BPT_cursor_t* BPT_crsHndl_t;

typedef struct {
	kvp_key_t key;
	pgIdx_t valuePgIdx;
} BPT_record_t;


BPT_Result_e BPT_cursorInit_All(BPT_hndl_t handle, BPT_crsHndl_t cursor, BPT_order_t order);
BPT_Result_e BPT_cursorInit_Range(BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t lower, kvp_key_t upper, BPT_order_t order) ;
BPT_Result_e BPT_cursorInit_Equal(BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t value);
BPT_Result_e BPT_cursorInit(BPT_hndl_t handle, BPT_crsHndl_t cursor);
BPT_cursorStatus_t BPT_cursorNext(BPT_crsHndl_t cursor, BPT_record_t *out_record);

//TSD_ENABLE_COMBINED_PERIOD
BPT_Result_e BPT_cursorPrevPeriod(BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t validLow, kvp_key_t validHigh);

#endif
#endif /* BPT_CURSOR_H */
