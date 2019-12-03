/*
 * utils.h
 *
 */

#ifndef UTILS_H_
#define UTILS_H_

#include "../env.h"
 uint32_t hexdec(const uint8_t *hex);
 void byteArrayToHex(uint8_t* binIn, uint8_t* wrtPtr, uint32_t binSize, uint8_t delimiter, bool nullTerminate);

 void reverseArray(uint8_t arr[], int start, int end);

 static inline uint32_t max(uint32_t a, uint32_t b) {
 	return a > b ? a : b;
 }
 static inline uint32_t min(uint32_t a, uint32_t b) {
 	return a > b ? b : a;
 }


#endif /* UTILS_H_ */
