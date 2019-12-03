#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>


typedef void* bufPtr;
typedef struct {
	bufPtr data;
	uint32_t dataLen;
} buf_t;

//#define EDB_IS_CONSTRAINED	!(_WIN32 || _WIN64 || __CYGWIN__ || __MINGW32__ || __CYGWIN32__ )


#define EDB_PRINT_I(...)	printf( __VA_ARGS__);puts("\n");fflush(stdout)
#define EDB_PRINT_E( ...)	printf(__VA_ARGS__);puts("\n");fflush(stdout)
#define EDB_PRINT_W( ...)	printf(__VA_ARGS__);puts("\n");fflush(stdout)
#define EDB_PRINT_T( ...)	printf(__VA_ARGS__);puts("\n");fflush(stdout)


#define EDB_PRINT_QA( ...)		printf(__VA_ARGS__);puts("\n");fflush(stdout)
#define EDB_PRINT_QA_PASS()			EDB_PRINT_QA("\t\t\t\t\t\tPass!\n");

#define FORCE_ENUM_SIZE

/*
 * Prevent compiler warning on unused argument
 */
#define UNUSED(x) __attribute__((unused))x

#define PRE_PACKED
#define POST_PACKED __attribute__((__packed__))

#endif
