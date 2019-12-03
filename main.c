/*
 ============================================================================
 Name        : main.c
 Author      : 
 Version     :
 Copyright   :
 Description :
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>


#include <edb/test.h>
#include <timeSeriesData/tsd.h>

int main(void) {

	EDB_test();

	TSD_init();
	TSD_test();

	return EXIT_SUCCESS;
}
