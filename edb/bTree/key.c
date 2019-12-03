/*
 * bpt_keyHelp.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/bTree/key.h>

char bpt_compare_uint32(void* first_key, void* second_key, int key_size) {
	if (*(uint32_t*) first_key < *(uint32_t*) second_key)
		return -1;
	else if (*(uint32_t*) first_key > *(uint32_t*) second_key)
		return 1;
	else
		return 0;
}


#endif
