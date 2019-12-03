/*
 * StagingBufferHeap.c
 *
 */

#include "config.h"
#if INCLUDE_TSD

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "env.h"
#include "os.h"

#include <timeSeriesData/prv.h>

static void* heap;
static stagedPoint_t* nextFree;
static uint32_t used;
static uint32_t available;

void tsd_allocStagingBufferHeap(uint32_t pointCount) {
	stagedPoint_t* p;

	heap = _OS_MAlloc(pointCount * sizeof(stagedPoint_t));
	used = 0;
	available = pointCount;

	nextFree = (stagedPoint_t*) heap;
	p = nextFree;
	for (uint32_t i = 0; i < pointCount - 1; i++) {
		p->next = p + 1;
		p++;
	}
	p[pointCount].next = NULL;
}
void tsd_freeStagingBufferHeap(){
	_OS_Free(heap);
	heap = NULL;
	used = 0;
	available = 0;
}
stagedPoint_t* tsd_getStagePoint() {
	if (used == available)
		return NULL;
	stagedPoint_t* sp = nextFree;
	nextFree = nextFree->next;
	used++;
	return sp;
}
void tsd_returnStagePoint(stagedPoint_t* sp) {
	sp->next = nextFree;
	nextFree = sp;
	used --;
}


#endif
