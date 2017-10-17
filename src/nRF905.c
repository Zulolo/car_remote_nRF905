/*
 * nRF905.c
 *
 *  Created on: Oct 17, 2017
 *      Author: zulolo
 *      Description: Use wiringPi ISR and SPI feature to control nRF905
 */
#include "wiringPi.h"
#include "wiringPiSPI.h"

static int nRF905SPI_CHN = 0;

static int nRF905SPI_DataWR(unsigned char *pData, int nDataLen){
	return wiringPiSPIDataRW(nRF905SPI_CHN, pData, nDataLen);
}

static int nRF905CRInitial(int nRF905SPI_Fd) {

}

int nRF905Initial(int nSPI_Channel, int nSPI_Speed) {
	int nRF905SPI_Fd = wiringPiSPISetup(nSPI_Channel, nSPI_Speed);
	nRF905SPI_CHN = nSPI_Channel;
	nRF905CRInitial(nRF905SPI_Fd);
}
