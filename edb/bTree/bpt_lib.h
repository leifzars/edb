/*
 * bpt_lib.h
 *
 *
 * bpt sourced from
 * https://github.com/iondbproject/iondb/blob/development/src/dictionary/bpp_tree/bpp_tree.c
 *
 * The core of the B++Tree is sourced from iondb, some simplifications
 * have been made.
 *
 *
 * Thread safe only when accessing unique FPM.
 * Must insure non concurrent access to same index, or
 * multiple indexes backed by the same FPM.
 *
 * Limitations
 * 	Key Type - UInt32 only
 * 	Value Type - UInt32 only
 * 	Page size - 512, should be easy to make dynamic
 *
 */

#ifndef BPT_LIB_H
#define BPT_LIB_H

#include <edb/bTree/bpt.h>
#include <edb/bTree/cursor.h>


#endif /* BPT_LIB_H */
