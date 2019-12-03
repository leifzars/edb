/*
 * os.h
 *
 */

#ifndef OS_H_
#define OS_H_

#include "lib/crc.h"


#define _OS_MAlloc(size) malloc(size)
#define _OS_Free(ptr)	free(ptr)

/*
 * Called periodically in the middle of a long running operation
 * Helpful with watchdog timers
 */
#define _EDB_BUSY()

/*
 * _OS_getPageSize()
 * Size of single block of storage. Should be dynamic.
 * BPT root node may not allow for adjustments.
 * May be hardwired for 512
 *  */
#define _OS_getPageSize() 	512
#define _OS_flushFat()
#define  __CRC32(ptr, size, start)	rc_crc32(start, ptr, size)


/*
 * Required for concurrent access
 */
#define _OS_SEMAPHORE_STRUC			void*
#define _OS_SEMAPHORE_OPEN(sem, val)
#define _OS_SEMAPHORE_CLOSE(sem)
#define _OS_SEMAPHORE_POST(sem)
#define _OS_SEMAPHORE_WAIT(sem)

#define _OS_MUTEX_TYPE				void*
#define _OS_MUTEX_INIT(ptr,initVal)
#define _OS_MUTEX_LOCK(ptr)			0
#define _OS_MUTEX_UNLOCK(ptr)		0


#endif /* OS_H_ */
