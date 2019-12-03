/*
 * utils.c
 *
 */
#include "utils.h"

static const long hextable[] = {
   [0 ... 255] = -1, // bit aligned access into this table is considerably
   ['0'] = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, // faster for most modern processors,
   ['A'] = 10, 11, 12, 13, 14, 15,       // for the space conscious, reduce to
   ['a'] = 10, 11, 12, 13, 14, 15        // signed char.
};


/**
 * @brief convert a hexidecimal string to a signed long
 * will not produce or process negative numbers except
 * to signal error.
 *
 * @param hex without decoration, case insensative.
 *
 * @return -1 on error, or result (max sizeof(long)-1 bits)
 */
uint32_t hexdec(const uint8_t *hex) {
	uint32_t ret = 0;
	while (*hex && ret >= 0) {
		ret = (ret << 4) | hextable[*hex++];
	}
	return ret;
}

void byteArrayToHex(uint8_t* binIn, uint8_t* wrtPtr, uint32_t binSize, uint8_t delimiter, bool nullTerminate){
	unsigned char * pin = binIn;
	    const char * hex = "0123456789ABCDEF";
	    char * pout = (char*)wrtPtr;
	    int i = 0;
	    for(; i < binSize-1; ++i){
	        *pout++ = hex[(*pin>>4)&0xF];
	        *pout++ = hex[(*pin++)&0xF];
	        if(delimiter)
	        	*pout++ = delimiter;
	    }
	    *pout++ = hex[(*pin>>4)&0xF];
	    *pout++ = hex[(*pin)&0xF];

	    if(nullTerminate)
	    	*pout = 0;
}
void reverseArray(uint8_t arr[], int start, int end)
{
	uint8_t temp;
    while (start < end)
    {
        temp = arr[start];
        arr[start] = arr[end];
        arr[end] = temp;
        start++;
        end--;
    }
}

