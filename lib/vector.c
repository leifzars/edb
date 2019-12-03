/*
 * vector.c
 *
 */

#include  "string.h"
#include "stdlib.h"

#include "../env.h"
#include "os.h"
#include "assert.h"
#include "vector.h"


#define GROWTH_FACTOR		1.5

void vec_init (vec_t* v, uint32_t itemSize, uint32_t initCapacity) {
	//if (initCapacity == 0)
	//	initCapacity = VECTOR_INIT_CAPACITY;

	v->itemSize = itemSize;
	v->capacity = initCapacity;
	v->size = 0;
	if (initCapacity)
		v->items = malloc(v->itemSize * initCapacity);
	else
		v->items = NULL;
}

int vec_size (vec_t *v) {
	return v->size;
}

void vec_clear(vec_t *v) {
	v->size = 0;
}

static void vec_upsize(vec_t *v) {
	if (v->capacity == 0)
		vec_resize(v, VECTOR_INIT_CAPACITY);
	else
		vec_resize(v, v->capacity * GROWTH_FACTOR);
}

void vec_resize (vec_t *v, uint32_t capacity) {
	void *newItems = malloc(v->itemSize * capacity);
	if (newItems) {
		if (v->items) {
			memcpy(newItems, v->items, v->itemSize * v->size);
			_OS_Free(v->items);
		}
		v->items = newItems;
		v->capacity = capacity;
	}
}

void* vec_addValue(vec_t *v, uint32_t value) {
	if (v->capacity == v->size)
	vec_upsize(v);
	void* wrtPtr = (uint8_t*)v->items + v->size * v->itemSize;
	*(uint32_t*)wrtPtr = value;
	//memcpy(wrtPtr, &value, v->itemSize);
	v->size++;
	return wrtPtr;
}

void* vec_add(vec_t *v, void *item) {
	if (v->capacity == v->size)
	vec_upsize(v);

	void* wrtPtr = (uint8_t*) v->items + v->size * v->itemSize;
	memcpy(wrtPtr, item, v->itemSize);
	v->size++;
	return wrtPtr;
}

void vec_add_sorted(vec_t *v, void* value, uint32_t valueOffset) {
	uint32_t nValueIndex = *(uint32_t*) ((uint8_t*) value + valueOffset);
	uint32_t cValueIndex;
	;
	void* cValue;
	uint32_t i;

	for (i = 0; i < v->size; i++) {
		cValue = vec_get(v, i);
		cValueIndex = *(uint32_t*) ((uint8_t*) cValue + valueOffset);
		if (cValueIndex > nValueIndex)
			break;

	}
	vec_insert(v, i, value);
}
void vec_add_sorted16(vec_t *v, void* value, uint32_t valueOffset) {
	uint16_t nValueIndex = *(uint16_t*) ((uint8_t*) value + valueOffset);
	uint16_t cValueIndex;
	;
	void* cValue;
	uint32_t i;

	for (i = 0; i < v->size; i++) {
		cValue = vec_get(v, i);
		cValueIndex = *(uint16_t*) ((uint8_t*) cValue + valueOffset);
		if (cValueIndex > nValueIndex)
			break;

	}
	vec_insert(v, i, value);
}
void vec_add_sorted64(vec_t *v, void* value, uint32_t valueOffset) {
	uint64_t nValueIndex = *(uint64_t*) ((uint8_t*) value + valueOffset);
	uint64_t cValueIndex;
	;
	void* cValue;
	uint32_t i;

	for (i = 0; i < v->size; i++) {
		cValue = vec_get(v, i);
		cValueIndex = *(uint64_t*) ((uint8_t*) cValue + valueOffset);
		if (cValueIndex > nValueIndex)
			break;

	}
	vec_insert(v, i, value);
}

void vec_sort16(vec_t *v, uint32_t valueOffset) {
	uint32_t c;

	uint8_t tmpPtr[v->itemSize];
	void* cPtr;

	void* dPtr;
	uint16_t dValue;
	void* dL1Ptr;
	uint16_t dL1Value;

	for (c = 1; c <= v->size - 1; c++) {
		cPtr = (void*) (((uint8_t*) v->items) + c * v->itemSize);

		dPtr = cPtr;
		dValue = *(uint16_t*) (dPtr + valueOffset);
		dL1Ptr = (dPtr - v->itemSize);
		dL1Value = *(uint16_t*) (dL1Ptr + valueOffset);
		//d = c;

		while ((dPtr != v->items) && dValue < dL1Value) {

			memcpy((void*) tmpPtr, dPtr, v->itemSize);
			memcpy(dPtr, dL1Ptr, v->itemSize);
			memcpy(dL1Ptr, (void*) tmpPtr, v->itemSize);

			dPtr = (dPtr - v->itemSize);
			dValue = *(uint16_t*) (dPtr + valueOffset);
			dL1Ptr = (dPtr - v->itemSize);
			dL1Value = *(uint16_t*) (dL1Ptr + valueOffset);
		}
	}
}

void vec_insert (vec_t* v, uint32_t index, void* item) {
	if (v->capacity == v->size)
		vec_upsize(v);

	void* wrtPtr = (uint8_t*) v->items + (index + 1) * v->itemSize;
	void* readPtr = (uint8_t*) v->items + index * v->itemSize;
	uint32_t mvSize = v->itemSize * (v->size - index);
	memmove(wrtPtr, readPtr, mvSize);

	wrtPtr = (uint8_t*) v->items + index * v->itemSize;
	memcpy(wrtPtr, item, v->itemSize);
	v->size++;
}
void vec_set (vec_t *v, uint32_t index, void *item) {
	if (index < v->size) {
		void* wrtPtr = (uint8_t*) v->items + index * v->itemSize;
		memcpy(wrtPtr, item, v->itemSize);
	}
}

void* vec_get(vec_t *v, uint32_t index) {
	if (index < v->size)
	return (void*)( ((uint8_t*) v->items) + index * v->itemSize);
	return NULL;
}

void vec_delete (vec_t *v, uint32_t index) {
	if (index >= v->size)
		return;

	//v->items[index] = NULL;
	if ((index + 1) < v->size) {
		void* wrtPtr = (uint8_t*) v->items + index * v->itemSize;
		void* readPtr = (uint8_t*) v->items + (index + 1) * v->itemSize;
		uint32_t mvSize = v->itemSize * (v->size - index - 1);
		memcpy(wrtPtr, readPtr, mvSize);
	}

	v->size--;
	if (v->size == 0) {
		_OS_Free(v->items);
		v->items = NULL;
		v->capacity = 0;
	} else if (v->size > 0 && v->size == v->capacity / 4)
		vec_resize(v, v->capacity / 2);
}

void vec_free (vec_t *v) {
	if (v->items)
		_OS_Free(v->items);
}

void* vec_find_binary(vec_t *v, uint32_t value, uint32_t offset) {
	int32_t first, last, middle;
	uint32_t middleValue;
	void* middlePtr;

	if (v->size == 0)
		return NULL;

	first = 0;
	last = (uint32_t) v->size - 1;
	middle = (first + last) / 2;

	while (first <= last) {
		middlePtr = (void*) ((uint8_t*) v->items + (v->itemSize * middle));
		middleValue = *(uint32_t*) ((uint8_t*) middlePtr + offset);

		if (middleValue < value)
			first = middle + 1;
		else if (middleValue == value) {
			return middlePtr;
		} else {
			last = middle - 1;
		}

		middle = (first + last) / 2;
	}
	//if (first > last)
	return NULL;
}
uint32_t vec_findIndex_binaryFirst(vec_t *v, uint32_t value, uint32_t offset) {
	uint32_t idx;
	uint32_t prevValue;

	idx = vec_findIndex_binary(v, value, offset);
	if (idx == VEC_NOT_FOUND)
		return VEC_NOT_FOUND;

	while (idx) {

		prevValue = *(uint32_t*) ((uint8_t*) v->items + (v->itemSize * (idx - 1)) + offset);
		if (prevValue != value)
			return idx;
		idx--;
	}
	return idx;
}

uint32_t vec_findIndex_binary(vec_t *v, uint32_t value, uint32_t offset) {
	int32_t first, last, middle;
	uint32_t middleValue;

	if (v->size == 0)
		return VEC_NOT_FOUND;

	first = 0;
	last = (uint32_t) v->size - 1;
	middle = (first + last) / 2;

	while (first <= last) {
		middleValue = *(uint32_t*) ((uint8_t*) v->items + (v->itemSize * middle) + offset);

		if (middleValue < value)
			first = middle + 1;
		else if (middleValue == value) {
			return middle;
		} else {
			last = middle - 1;
		}

		middle = (first + last) / 2;
	}
	//if (first > last)
	return VEC_NOT_FOUND;
}

