/*
 ============================================================================
 Name        : car_remote_nRF905.c
 Author      : ZK
 Version     :
 Copyright   : Your copyleft notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wiringPi.h"
#include "nRF905Handler.h"

#include "system.h"

#define NRF905_SPI_CHN			0
#define NRF905_SPEED			5000000

static const uint16_t unCAR_REMOTE_HOPPING_TAB[] = { 0x884C, 0x883A, 0x8846, 0x8832, 0x884A, 0x8835,
		0x884B, 0x8837, 0x884F, 0x883E, 0x8847, 0x8838, 0x8844, 0x8834, 0x8843, 0x8834, 0x884B,
		0x8839, 0x884D, 0x883A, 0x884E, 0x883C, 0x8832, 0x883F };

PI_THREAD (nRF905Thread)
{
	(void)piHiPri(10);
	nRemoteCarStartReceive(NRF905_SPI_CHN, NRF905_SPEED, unCAR_REMOTE_HOPPING_TAB);
}

int main(void) {
	int32_t nRF905ThreadID;
	puts("!!!Hello my remote car!!!"); /* prints !!!Hello my remote car!!! */

	nRF905ThreadID = piThreadCreate(nRF905Thread) ;
	if (nRF905ThreadID != 0) {
		REMOTE_CAR_LOG_ERR("nRF905 receive thread start error.");

	}

	return EXIT_SUCCESS;
}
