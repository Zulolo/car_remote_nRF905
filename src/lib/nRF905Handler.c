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


int32_t nRemoteCarControl(uint8_t* pData) {
	return 0;
}

int32_t nRemoteCarStartReceive(int32_t nSPI_Channel, int32_t nSPI_Speed, const uint16_t* pHoppingTable) {
	uint8_t unRF_Frame[32];

	nClearSystemValue(REMOTE_CAR_SYS_INFO_RF_FRAME_ERR);
	if (nRF905Initial(nSPI_Channel, nSPI_Speed) != 0) {
		REMOTE_CAR_LOG_ERR("Initial nRF905 SPI for remote car error.");
		nSetSystemValue(REMOTE_CAR_SYS_INFO_RF_STATUS, "RF init error");
		return (-1);
	}
	nRF905StartListen(pHoppingTable);
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
