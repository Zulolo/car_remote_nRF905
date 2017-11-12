/*
 ============================================================================
 Name        : car_remote_nRF905.c
 Author      : ZK
 Version     :
 Copyright   : Your copyleft notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <nRFxxxHandler.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wiringPi.h"
#include "system.h"

PI_THREAD (nRF905Thread)
{
	nRemoteCarStartReceive();
	return NULL ;
}

int main(void) {
	int32_t nRF905ThreadID;
	puts("!!!Hello my remote car!!!"); /* prints !!!Hello my remote car!!! */

	nRF905ThreadID = piThreadCreate(nRF905Thread) ;
	if (nRF905ThreadID != 0) {
		REMOTE_CAR_LOG_ERR("nRF905 receive thread start error.");
		return EXIT_FAILURE;
	}
	while (1) {
		sleep(2);

	}
	return EXIT_SUCCESS;
}
