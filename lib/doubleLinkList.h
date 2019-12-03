#include "../env.h"
//LZ added Q (FIFO)
/** \addtogroup lib
    @{ */
/**
 * \defgroup list Linked list library
 *
 * The linked list library provides a set of functions for
 * manipulating linked lists.
 *
 * A linked list is made up of elements where the first element \b
 * must be a pointer. This pointer is used by the linked list library
 * to form lists of the elements.
 *
 * Lists are declared with the LIST() macro. The declaration specifies
 * the name of the list that later is used with all list functions.
 *
 * Lists can be manipulated by inserting or removing elements from
 * either sides of the list (linkList_push(), linkList_add(), linkList_pop(),
 * linkList_chop()). A specified element can also be removed from inside a
 * list with linkList_remove(). The head and tail of a list can be
 * extracted using linkList_head() and linkListQueue_tail(), respectively.
 *
 * @{
 */

/**
 * \file
 * Linked list manipulation routines.
 * \author Adam Dunkels <adam@sics.se>
 *
 *
 */



/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef DOUBLE_LINK_LIST_Q_H_
#define DOUBLE_LINK_LIST_Q_H_
//#include "env/types.h"

/**
 * Declare a linked list.
 *
 * This macro declares a linked list with the specified \c type. The
 * type \b must be a structure (\c struct) with its first element
 * being a pointer. This pointer is used by the linked list library to
 * form the linked lists.
 *
 * The list variable is declared as static to make it easy to use in a
 * single C module without unnecessarily exporting the name to other
 * modules.
 *
 * \param name The name of the list.
 */
#define DLIST(name) \
         static void *LIST_CONCAT(name,_list) = NULL; \
         static linkListQueue_t name = (linkListQueue_t)&LIST_CONCAT(name,_list)

/**
 * Declare a linked list inside a structure declaraction.
 *
 * This macro declares a linked list with the specified \c type. The
 * type \b must be a structure (\c struct) with its first element
 * being a pointer. This pointer is used by the linked list library to
 * form the linked lists.
 *
 * Internally, the list is defined as two items: the list itself and a
 * pointer to the list. The pointer has the name of the parameter to
 * the macro and the name of the list is a concatenation of the name
 * and the suffix "_list". The pointer must point to the list for the
 * list to work. Thus the list must be initialized before using.
 *
 * The list is initialized with the LIST_STRUCT_INIT() macro.
 *
 * \param name The name of the list.
 */
#define DLIST_STRUCT(name) \
         void *LIST_CONCAT(name,_list); \
         linkListQueue_t name

/**
 * Initialize a linked list that is part of a structure.
 *
 * This macro sets up the internal pointers in a list that has been
 * defined as part of a struct. This macro must be cadllEd before using
 * the list.
 *
 * \param struct_ptr A pointer to the struct
 * \param name The name of the list.
 */
#define DLIST_STRUCT_INIT(struct_ptr, name)                              \
    do {                                                                \
       (struct_ptr)->name = &((struct_ptr)->LIST_CONCAT(name,_list));   \
       (struct_ptr)->LIST_CONCAT(name,_list) = NULL;                    \
       linkList_init((struct_ptr)->name);                               \
    } while(0)


#define DLLQ_IS_FULL(list)	(list->length == list->maxLength)


#define FOR_EACH_dll_ELEMENT(LIST_PTR, VAR, VAR_PTR_TYPE)\
		VAR_PTR_TYPE VAR;\
		for(VAR = (LIST_PTR)->head; VAR != NULL; VAR = (VAR_PTR_TYPE)(VAR == (LIST_PTR)->tail ? NULL : (VAR->next)))

#define FOR_EACH_dll_ELEMENT2(LIST_PTR, VAR)\
		for(dllElementPtr_t VAR = (LIST_PTR)->head; VAR != NULL; VAR = (dllElementPtr_t)(VAR == (LIST_PTR)->tail ? NULL : (VAR->next)))



/**
 * The linked list type.
 *
 */
struct dllElement {
	struct dllElement *next;
	struct dllElement *prev;
};
typedef struct dllElement* dllElementPtr_t;

typedef struct dllElement dllElement_t;

typedef struct {
	struct dllElement* head;
	struct dllElement* tail;
	uint16_t length;
	uint16_t maxLength;
}doubleLinkList_t, *doubleLinkListPtr_t;

void   dll_init(doubleLinkList_t* listPtr, uint16_t maxLength);
void * dll_pop (doubleLinkList_t* listPtr);
bool   dll_push(doubleLinkList_t* listPtr, dllElementPtr_t item);

//void * linkList_chop(linkListQueue_t* listPtr);

bool   dll_add(doubleLinkList_t* listPtr, dllElementPtr_t item);
void   dll_remove(doubleLinkList_t* listPtr, dllElementPtr_t item);
bool   dll_insert(doubleLinkList_t* listPtr, dllElementPtr_t previtem, dllElementPtr_t newitem);

void * dll_item_next(void *item);

//void   linkList_copy(linkListQueue_t* dest, linkListQueue_t src);



#endif /* LIST_H_ */

/** @} */
/** @} */
