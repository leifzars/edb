/*
 * config_errors.h
 *
 */

#ifndef EDB_CONFIG_ERRORS_H_
#define EDB_CONFIG_ERRORS_H_

#if (EDB_TEST_PAGE_OPS || EDB_TEST_BPT_OPS || EDB_TEST_OBJ_OPS) && EDB_ENABLE_X
#error "Disable transaction support to run low level tests"
#endif

#if EDB_QA_X && !EDB_ENABLE_X
#error "EDB_ENABLE_X Must be enabled"
#endif

#if EDB_ENABLED_NESTED_WRITE_X && !EDB_ENABLED_NESTED_X
#error "EDB_ENABLED_NESTED_X must be enabled, for EDB_ENABLED_NESTED_WRITE_X"
#endif

#if EDB_INCLUDE_EXT_WAL_DIAG && !EDB_INCLUDE_PAGE_DIAG
#error "EDB_ENABLE_DIAG_WAL needs EDB_INCLUDE_PAGE_DIAG enabled"
#endif



#endif /* EDB_CONFIG_ERRORS_H_ */
