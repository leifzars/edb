/*
 * assert.h
 *
 */

#ifndef SOURCES_LIB_ASSERT_H_
#define SOURCES_LIB_ASSERT_H_

#include "stdbool.h"
#include "stdint.h"


#define _ASSERT_DEBUG(condition)	_assert(condition)

#define _ASSERT(err) 	_assert(err)


void _assert(uint32_t condition);


#endif /* SOURCES_LIB_ASSERT_H_ */
