/*
 * vector.h
 *
 */

#ifndef LIB_VECTOR_H
#define LIB_VECTOR_H

#include "../env.h"

#define VEC_NOT_FOUND			0xFFFFFFFF


#define VECTOR_INIT_CAPACITY 8

#define VEC_INIT(vec, elementSize, initSize) vec_t vec; vec_init((&vec), elementSize, initSize)
#define VEC_ADD(vec, item) vec_add(vec, (void *) item)


#define VEC_SET(vec, id, item) vec_set(vec, id, (void *) item)
//#define VEC_GET(vec, type, id) ((type) vec_get(vec, id))
#define VEC_GET_(vec, index)	 ( ((uint8_t*) (vec)->items) + ((index) * (vec)->itemSize))
#define VEC_GET_PTR(vec, type, index)	((type) VEC_GET_(vec, index) )
#define VEC_GET_VAL(vec, type, index)	(*(type*)VEC_GET_(vec, index) )
#define VEC_ADD_VAL(vec, type, val)	\
do{\
if ((vec)->capacity == (vec)->size);		\
	vec_resize(vec, (vec)->capacity * 2);	\
	((type*)(vec)->items)[(vec)->size] = val;	\
	(vec)->size++;								\
} while (0)
#define VEC_DELETE(vec, id) vec_delete(vec, id)
#define VEC_SIZE(vec) vec_size(vec)
#define VEC_FREE(vec) vec_free(vec)


	typedef struct {
		void *items;
		uint32_t capacity;
		uint32_t size;
		uint32_t itemSize;
	}vec_t;

void vec_init(vec_t* v, uint32_t itemSize, uint32_t initCapacity);
int vec_size(vec_t*);

void* vec_addValue(vec_t*, uint32_t value);
void* vec_add(vec_t*, void *);
void vec_add_sorted(vec_t *v, void* value, uint32_t valueOffset);
void vec_add_sorted16(vec_t *v, void* value, uint32_t valueOffset);
void vec_add_sorted64(vec_t *v, void* value, uint32_t valueOffset);
void vec_insert(vec_t*, uint32_t index, void *);
void vec_set(vec_t*, uint32_t, void *);
void *vec_get(vec_t*, uint32_t);
void vec_delete(vec_t*, uint32_t);
void vec_free(vec_t*);
void vec_resize(vec_t *v, uint32_t capacity);
void vec_clear(vec_t *v);

void vec_sort16(vec_t *v, uint32_t valueOffset);

void* vec_find_binary(vec_t *v, uint32_t value, uint32_t offset);
uint32_t vec_findIndex_binary(vec_t *v, uint32_t value, uint32_t offset);
uint32_t vec_findIndex_binaryFirst(vec_t *v, uint32_t value, uint32_t offset);


#endif /* LIB_VECTOR_H */
