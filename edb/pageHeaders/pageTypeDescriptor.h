/*
 * pageTypeDescriptor.h
 *
 */

#ifndef EDB_PAGEHEADERS_PAGETYPEDESCRIPTION_H
#define EDB_PAGEHEADERS_PAGETYPEDESCRIPTION_H


#include <edb/config.h>
#if INCLUDE_EDB
#include "../../env.h"

#include "pageHeader.h"

typedef struct {
	pageType_e type;
	uint8_t isObject :1;
	uint8_t isObjectOverFlow :1;
	uint16_t headerSize;
} fpm_valuePageHeader_t;


extern fpm_valuePageHeader_t fpm_pgTypeInfos[];

#endif
#endif /* EDB_PAGEHEADERS_PAGETYPEDESCRIPTION_H */
