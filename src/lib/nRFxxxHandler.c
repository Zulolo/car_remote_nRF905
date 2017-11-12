/*
 * nRFxxxHandler.c
 *
 *  Created on: Oct 18, 2017
 *      Author: zulolo
 */

#include <nRFxxx.h>
#include <stdint.h>
#include <stdio.h>
#include "system.h"

#define REMOTE_CAR_SYS						"remote_car:"
#define REMOTE_CAR_SYS_INFO					REMOTE_CAR_SYS "info:"
#define REMOTE_CAR_SYS_INFO_RF_STATUS		REMOTE_CAR_SYS_INFO "rf_status"
#define REMOTE_CAR_SYS_INFO_RF_FRAME_ERR	REMOTE_CAR_SYS_INFO "rf_err_cnt"

#define NRFxxx_SPI_CHN						0
#define NRFxxx_SPEED						8000000

static const uint16_t unCAR_REMOTE_HOPPING_TAB[] = { 0x804C, 0x803A, 0x8046, 0x8032, 0x804A, 0x8035,
		0x804B, 0x8037, 0x804F, 0x803E, 0x8047, 0x8038, 0x8044, 0x8034, 0x8043, 0x8034, 0x804B,
		0x8039, 0x804D, 0x803A, 0x804E, 0x803C, 0x8032, 0x803F };

int32_t nRemoteCarControl(uint8_t* pData) {
	return 0;
}

int32_t nRemoteCarStartReceive(void) {
	uint8_t unRF_Frame[NRFxxx_RX_PAYLOAD_LEN];

//	printf("nClearSystemValue start.\n");
	nClearSystemValue(REMOTE_CAR_SYS_INFO_RF_FRAME_ERR);
//	printf("nRFxxxInitial start.\n");
	if (nRFxxxInitial(NRFxxx_SPI_CHN, NRFxxx_SPEED, 3) != 0) {
		REMOTE_CAR_LOG_ERR("Initial nRFxxx SPI for remote car error.");
		nSetSystemValue(REMOTE_CAR_SYS_INFO_RF_STATUS, "RF init error");
		return (-1);
	}
	nRFxxxStartListen(unCAR_REMOTE_HOPPING_TAB, sizeof(unCAR_REMOTE_HOPPING_TAB)/sizeof(uint16_t));
	while (1) {
		if (nRFxxxReadFrame(unRF_Frame, sizeof(unRF_Frame)) > 0) {
//			printf("Remote car received: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X.\n",
//					unRF_Frame[0], unRF_Frame[1], unRF_Frame[2], unRF_Frame[3], unRF_Frame[4]);
			if (nRemoteCarControl(unRF_Frame) != 0) {
				REMOTE_CAR_LOG_ERR("Parse received data error.");
				nIncrSystemValue(REMOTE_CAR_SYS_INFO_RF_FRAME_ERR);
			} else {
				nRFxxxSendFrame(unRF_Frame, sizeof(unRF_Frame));
			}
		} else {
			printf("Remote car receive data from nRFxxx error.\n");
			REMOTE_CAR_LOG_ERR("Remote car receive data from nRFxxx error.");
			nSetSystemValue(REMOTE_CAR_SYS_INFO_RF_STATUS, "RF receive data error");
			return (-1);
		}
	}
	return 0;
}
