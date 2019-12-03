/**
 * \addtogroup list
 * @{
 */

/**
 * \file
 * Linked list library implementation.
 *
 * \author Adam Dunkels <adam@sics.se>
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
#include "doubleLinkList.h"
#include "assert.h"

/*---------------------------------------------------------------------------*/
/**
 * Initialize a list.
 *
 * This function initalizes a list. The list will be empty after this
 * function has been called.
 *
 * \param list The list to be initialized.
 */
void dll_init(doubleLinkList_t* list, uint16_t maxLength) {
	list->head = NULL;
	list->tail = NULL;
	list->length = 0;
	list->maxLength = maxLength;
}

/*---------------------------------------------------------------------------*/
/**
 * Add an item at the end of a list.
 *
 * This function adds an item to the end of the list.
 *
 * \param list The list.
 * \param item A pointer to the item to be added.
 *
 * \sa linkList_push()
 *
 */
bool dll_add(doubleLinkList_t* list, dllElementPtr_t newItem) {
	if (list->length == list->maxLength)
		return false;

	if (list->length) {
		newItem->prev = list->tail;
		list->tail->next = newItem;
		list->tail = newItem;
	} else {
		list->head = list->tail = newItem;
		newItem->prev = NULL;
	}
	newItem->next = NULL;

	list->length++;

	return true;
}
/*---------------------------------------------------------------------------*/
/**
 * Add an item to the start of the list.
 */
bool dll_push(doubleLinkList_t* list, dllElementPtr_t newItem) {
	//_ASSERT_DEBUG(newItem->next == NULL);
	//_ASSERT_DEBUG(newItem->prev == NULL);

	if (list->length == list->maxLength)
		return false;

	if (list->length) {
		newItem->next = list->head;

		list->head->prev = newItem;
		newItem->prev = NULL;
	} else {
		list->tail = newItem;
		newItem->next = NULL;

		newItem->prev = NULL;
	}

	list->head = newItem;

	++list->length;
	return true;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove the first object on a list.
 *
 * This function removes the first object on the list and returns a
 * pointer to it.
 *
 * \param list The list.
 * \return Pointer to the removed element of list.
 */
/*---------------------------------------------------------------------------*/
void *
dll_pop(doubleLinkList_t* list) {
	dllElementPtr_t l;
	l = list->head;
	if (l) {
		list->length--;

		if (list->length) {
			list->head = list->head->next;

			list->head->prev = NULL;
		} else {
			list->head = list->tail = NULL;
		}
	}
	return l;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove a specific element from a list.
 *
 * This function removes a specified element from the list.
 *
 * \param list The list.
 * \param item The item that is to be removed from the list.
 *
 */
/*---------------------------------------------------------------------------*/

void dll_remove(doubleLinkList_t* list, dllElementPtr_t item) {
	dllElementPtr_t next = item->next;
	dllElementPtr_t prev = item->prev;

	_ASSERT_DEBUG(item != NULL);
	_ASSERT_DEBUG(list->length > 0);

	if (prev != NULL && next != NULL) {
		prev->next = next;
		next->prev = prev;
	}else if(prev == NULL && next == NULL){
		list->head = NULL;
			list->tail = NULL;
	} else if (prev == NULL) { // OR list->head == item
		list->head = next;
		next->prev = NULL;
	} else{//else if (next == NULL) { // OR list->tail == item
		list->tail = prev;
		prev->next = NULL;
	}

	--list->length;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief      Insert an item after a specified item on the list
 * \param list The list
 * \param previtem The item after which the new item should be inserted
 * \param newitem  The new item that is to be inserted
 * \author     Adam Dunkels
 *
 *             This function inserts an item right after a specified
 *             item on the list. This function is useful when using
 *             the list module to ordered lists.
 *
 *             If previtem is NULL, the new item is placed at the
 *             start of the list.
 *
 */
bool dll_insert(doubleLinkList_t* list, dllElementPtr_t prevItem, dllElementPtr_t newItem) {
	if (list->length == list->maxLength)
		return false;

	dllElementPtr_t next;
	dllElementPtr_t prev;


	if (prevItem == NULL) {
		next = list->head;
		prev = NULL;
	}else{
		next = prevItem->next;
		prev = prevItem->prev;
	}

	if(prev != NULL)
		prev->next = newItem;
	else
		list->head = newItem;
	newItem->prev = prev;

	if(next != NULL)
		next->prev = newItem;
	else
		list->tail = newItem;
	newItem->next = next;

	++list->length;
	return true;
}

/*---------------------------------------------------------------------------*/
/** @} */
