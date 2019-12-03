/*
 * key.h
 *
 */

#ifndef BPT_KEY_H
#define BPT_KEY_H

#include <edb/config.h>
#if INCLUDE_EDB
#include "../../env.h"

char bpt_compare_uint32(void* first_key, void* second_key, int key_size);

#endif
#endif /*BPT_KEY_H*/
