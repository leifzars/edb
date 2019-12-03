/*
 * cursor.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/bTree/nodeAccess.h>
#include  <edb/bTree/cursor.h>

/*
 * test_predicate
 * Should be a variable declared in BPT_hndl_t
 */
static bool test_predicate(BPT_crsHndl_t cursor, kvp_key_t key);

BPT_Result_e BPT_cursorInit_All(BPT_hndl_t handle, BPT_crsHndl_t cursor, BPT_order_t order) {
	cursor->predicate.type = BPT_PT_ALL;
	cursor->predicate.order = order;
	return BPT_cursorInit(handle, cursor);
}
BPT_Result_e BPT_cursorInit_Range(BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t lower, kvp_key_t upper, BPT_order_t order) {
	cursor->predicate.type = BPT_PT_RANGE;
	cursor->predicate.range.lower = lower;
	cursor->predicate.range.upper = upper;
	cursor->predicate.order = order;
	return BPT_cursorInit(handle, cursor);
}

BPT_Result_e BPT_cursorInit_Equal(BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t value) {
	cursor->predicate.type = BPT_PT_EQUAL;
	cursor->predicate.equality_value = value;
	cursor->predicate.order = BPT_PT_ACS;
	return BPT_cursorInit(handle, cursor);
}

BPT_Result_e BPT_cursorInit(BPT_hndl_t handle, BPT_crsHndl_t cursor) {
	BPT_Result_e bErr;
	kvp_key_t target_key;
	BPT_t *h = handle;

	if ((bErr = (BPT_Result_e) edb_pm_operatinalState(h->pm)))
		return bErr;

	bpt_syncRoot(handle);

	cursor->bpt = handle;
	cursor->operationNumberOnStart = cursor->bpt->operationNumber;

	cursor->status = BPT_CS_CURSOR_UNINITIALIZED;

	switch (cursor->predicate.type) {
	case BPT_PT_EQUAL: {
		target_key = cursor->predicate.equality_value;

		bErr = BPT_findKey(handle, &target_key, &cursor->bptCursor);

		if (bErr == BPT_NOT_FOUND) {
			cursor->status = BPT_CS_END_OF_RESULTS;
		} else if (bErr) {
			cursor->status = BPT_CS_END_OF_RESULTS_ON_ERR;
		} else {
			cursor->status = BPT_CS_CURSOR_INITIALIZED;
		}
		break;
	}

	case BPT_PT_RANGE: {
		if( cursor->predicate.order == BPT_PT_ACS){
			target_key = cursor->predicate.range.lower;
			bErr = BPT_findfFirstGreaterOrEqual(handle, &target_key, &cursor->bptCursor);
		}else{
			target_key = cursor->predicate.range.upper;
			bErr = BPT_findLastLessOrEqual(handle, &target_key, &cursor->bptCursor);
		}

		/* We search for the FGEQ of the Lower bound. */
		if (bErr) {
			cursor->status = BPT_CS_END_OF_RESULTS_ON_ERR;
		} else if (!test_predicate(cursor, cursor->bptCursor.key)) {
			/* If the key returned doesn't satisfy the predicate, we can exit */
			cursor->status = BPT_CS_END_OF_RESULTS;
		} else {
			cursor->status = BPT_CS_CURSOR_INITIALIZED;
		}
		break;
	}
	case BPT_PT_ALL: {
		/* We search for first key in B++ tree. */
		if ((bErr = BPT_findFirstKey(handle, &cursor->bptCursor))) {
			cursor->status = BPT_CS_END_OF_RESULTS_ON_ERR;
		} else {
			cursor->status = BPT_CS_CURSOR_INITIALIZED;
		}
		break;
	}

	default: {
		return BPT_ERROR;
	}
	}

	return BPT_OKAY;
}

//TSD_ENABLE_COMBINED_PERIOD
BPT_Result_e BPT_cursorPrevPeriod(BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t validLow, kvp_key_t validHigh) {
	BPT_Result_e bErr;
	bpt_cursor_t tmpCrs;

	tmpCrs = cursor->bptCursor;

	if ((bErr = BPT_findPrevKey(handle, &cursor->bptCursor))) {
		return bErr;
	}
	if (cursor->bptCursor.key < validLow || cursor->bptCursor.key > validHigh) {
		cursor->bptCursor = tmpCrs;
	} else {
		//if(cursor->status == BPT_CS_END_OF_RESULTS)
		cursor->status = BPT_CS_CURSOR_INITIALIZED;
		return BPT_OKAY;
	}

	return BPT_OKAY;
}

BPT_cursorStatus_t BPT_cursorNext(BPT_crsHndl_t cursor, BPT_record_t *out_record) {
	BPT_Result_e bErr;

	BPT_t *h = cursor->bpt;
	EDB_Hndl_t fpm = h->pm;

	if (fpm->isInBadSate)
		return BPT_CS_END_OF_RESULTS_ON_ERR;
	if (cursor->operationNumberOnStart != cursor->bpt->operationNumber) {
		_ASSERT_DEBUG(0);
		cursor->status = BPT_CS_STORE_CHANGED;
		return BPT_CS_STORE_CHANGED;
	}

	if (cursor->status == BPT_CS_CURSOR_UNINITIALIZED) {
		return cursor->status;
	} else if (cursor->status == BPT_CS_END_OF_RESULTS) {
		return cursor->status;
	} else if ((cursor->status == BPT_CS_CURSOR_INITIALIZED) || (cursor->status == BPT_CS_CURSOR_ACTIVE)) {
		if (cursor->status == BPT_CS_CURSOR_ACTIVE) {

			if(cursor->predicate.type == BPT_PT_EQUAL ){
				if (!h->dupKeys){
					goto EndOfResults;
				}
			}

			if( cursor->predicate.order == BPT_PT_ACS)
				bErr = BPT_findNextKey(h, &cursor->bptCursor);
			else
				bErr = BPT_findPrevKey(h, &cursor->bptCursor);

			if (bErr)
				goto EndOfResults;

			if(cursor->predicate.type == BPT_PT_EQUAL || cursor->predicate.type == BPT_PT_RANGE){
				if(test_predicate(cursor, cursor->bptCursor.key) == false){
					goto EndOfResults;
				}
			}

		} else {
			/* The status is cs_cursor_initialized */
			cursor->status = BPT_CS_CURSOR_ACTIVE;
		}

		/* Get key */
		out_record->key = cursor->bptCursor.key;
		out_record->valuePgIdx = cursor->bptCursor.valueOffset;

		return cursor->status;

		EndOfResults:
			cursor->status = BPT_CS_END_OF_RESULTS;
				return cursor->status;
	}

	return BPT_CS_INVALID_CURSOR;
}

bool test_predicate(BPT_crsHndl_t cursor, kvp_key_t key) {

	switch (cursor->predicate.type) {
	case BPT_PT_EQUAL: {
		return key == cursor->predicate.equality_value;

	}
	case BPT_PT_RANGE:
	{
		kvp_key_t lower_b = cursor->predicate.range.lower;
		kvp_key_t upper_b = cursor->predicate.range.upper;

		return key >= lower_b && key <= upper_b;
	}

	case BPT_PT_ALL: {
		return true;
	}
	}

	return false;
}

#endif
