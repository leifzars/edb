/*
 * pm_s.h
 *
 */

#ifndef EDB_PAGEMGR_PM_S_H
#define EDB_PAGEMGR_PM_S_H

#include "../../env.h"
typedef enum
	 {
		EDB_OKAY = 0,
	EDB_OKAY_NULL,
	EDB_OKAY_UNINITIALIZED,
	EDB_OKAY_DOES_NOT_EXIST,

	EDB_ERROR,
	EDB_NOT_FOUND, EDB_DELETED, EDB_ERROR_DUPLICATE_KEY, EDB_ERROR_INSERT_FAILED,
	EDB_ALREADY_EXISTS,

	EDB_ALREADY_EXISTS_DIFF_TYPE,
	EDB_CORRUPTED,
	EDB_ERROR_BUFFER_UNDER_SIZED,

	EDB_PM_EXCEEDS_EOF_VALUE,
	EDB_PM_PAGE_CORRUPTED,
	EDB_PM_PAGE_CORRUPTED_CRC,
	EDB_PM_PAGE_NOT_IN_CACHE,
	EDB_PM_ERROR,
	EDB_PM_OUT_OF_MEMORY,
	EDB_PM_LENGTH_CAN_NOT_BE_ZERO,
	EDB_PM_IO_OPEN_ERROR,
	EDB_PM_IO_ERROR,
	EDB_PM_IO_ERROR_FULL,
	EDB_PM_OPEN_ERROR,

	EDB_PM_FAILED_BAD_STATE,
	EDB_PM_FAILED_READ_ONLY,
	EDB_PM_NO_PREVIOUS_STATE,
	EDB_PM_ERROR_ILLEGAL_OPERATION,
	EDB_PM_ERR_INVALID_PARAMETER,
	EDB_PM_ERROR_FULL,
	EDB_PM_ERROR_UN_RECOVERABLE,
	EDB_PM_ERROR_UNASSOCIATED,
	EDB_PM_ERROR_PAGE_DOES_NOT_EXIST,
	EDB_PM_ERROR_UNKNOWN_FORMAT,

	//
	EDB_WAL_OVERFLOW,
	EDB_WAL_DIRTY, //Did not end cleanly
	EDB_WAL_FRMT_VIOLATION,
	EDB_WAL_NOT_ENABLED,

} EDB_result_t;


#endif /* EDB_PAGEMGR_PM_S_H */