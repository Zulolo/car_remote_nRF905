/*
 * nRF905Handler.c
 *
 *  Created on: Oct 18, 2017
 *      Author: zulolo
 */

#include <stdint.h>
#include "system.h"
#include "nRF905.h"

#define REMOTE_CAR_SYS						"remote_car:"
#define REMOTE_CAR_SYS_INFO					REMOTE_CAR_SYS "info:"
#define REMOTE_CAR_SYS_INFO_RF_STATUS		REMOTE_CAR_SYS_INFO "rf_status"
#define REMOTE_CAR_SYS_INFO_RF_FRAME_ERR	REMOTE_CAR_SYS_INFO "rf_err_cnt"

#define NRF905_SPI_CHN						0
#define NRF905_SPEED						5000000

static const uint16_t unCAR_REMOTE_HOPPING_TAB[] = { 0x884C, 0x883A, 0x8846, 0x8832, 0x884A, 0x8835,
		0x884B, 0x8837, 0x884F, 0x883E, 0x8847, 0x8838, 0x8844, 0x8834, 0x8843, 0x8834, 0x884B,
		0x8839, 0x884D, 0x883A, 0x884E, 0x883C, 0x8832, 0x883F };

int32_t nRemoteCarControl(uint8_t* pData) {
	return 0;
}

int32_t nRemoteCarStartReceive(void) {
	uint8_t unRF_Frame[32];

	nClearSystemValue(REMOTE_CAR_SYS_INFO_RF_FRAME_ERR);
	if (nRF905Initial(NRF905_SPI_CHN, NRF905_SPEED) != 0) {
		REMOTE_CAR_LOG_ERR("Initial nRF905 SPI for remote car error.");
		nSetSystemValue(REMOTE_CAR_SYS_INFO_RF_STATUS, "RF init error");
		return (-1);
	}
	nRF905StartListen(unCAR_REMOTE_HOPPING_TAB, sizeof(unCAR_REMOTE_HOPPING_TAB)/sizeof(uint16_t));
	while (1) {
		if (nRF905ReadFrame(unRF_Frame, sizeof(unRF_Frame)) > 0) {
			if (nRemoteCarControl(unRF_Frame) != 0) {
				REMOTE_CAR_LOG_ERR("Parse received data error.");
				nIncrSystemValue(REMOTE_CAR_SYS_INFO_RF_FRAME_ERR);
			}
		} else {
			REMOTE_CAR_LOG_ERR("Remote car receive data from nRF905 error.");
			nSetSystemValue(REMOTE_CAR_SYS_INFO_RF_STATUS, "RF receive data error");
			return (-1);
		}
	}
	return 0;
}
