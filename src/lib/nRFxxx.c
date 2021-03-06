/*
 * nRFxxx.c
 *
 *  Created on: Oct 17, 2017
 *      Author: zulolo
 *      Description: Use wiringPi ISR and SPI feature to control nRFxxx
 */
//#include <stdint.h>
#include <sys/time.h>       /* for setitimer */
#include <signal.h>     /* for signal */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wiringPi.h"
#include "wiringPiSPI.h"
#include "system.h"
#include "nRFxxx.h"

#define NRFxxx_TX_ADDR_LEN				4
#define NRFxxx_RX_ADDR_LEN				4
#define TEST_NRFxxx_TX_ADDR				0x87654321
#define TEST_NRFxxx_RX_ADDR				0x12345678

/* Here is how the RF works:
 * UP keeps monitoring if there is any valid frame available (CD, AM, DR should be all SET) on certain channel for 300ms.
 * If yes, receive frame and continuously monitoring on this channel. If no, hop to next channel according to the hopping table.
 *
 * Down keeps transmitting frames every 100ms. If transmitting failed, (can not get valid response) down start hopping procedure.
 * Hopping procedure is to trying burst transmitting ACK frame continuously.
 * If transmitting failed, jump to next channel according to table and start to transmit again.
 * The TX&RX address is generated by some algorithm and is set at start up or during hopping
 *
 */
#ifdef NRF905_AS_RF
#define NRFxxx_DR_PIN_ISR_EDGE					INT_EDGE_RISING
#define NRFxxx_TX_EN_PIN						0
#define NRFxxx_TRX_CE_PIN						2
#define NRFxxx_PWR_UP_PIN						3
#define NRFxxx_DR_PIN							4
#define NRFxxx_RX_ADDRESS_IN_CR					5
#define NRFxxx_CMD_WC_MASK						0x0F
#define NRFxxx_CMD_WC(unWR_CFG_ByteIndex)		((unWR_CFG_ByteIndex) & NRFxxx_CMD_WC_MASK)	// Write Configuration register
#define NRFxxx_CMD_RC_MASK						0x0F
#define NRFxxx_CMD_RC(unRD_CFG_ByteIndex)		(((unRD_CFG_ByteIndex) & NRFxxx_CMD_RC_MASK) | 0x10)	// Read Configuration register
#define NRFxxx_CMD_WTP							0x20
#define NRFxxx_CMD_RTP							0x21
#define NRFxxx_CMD_WTA							0x22
#define NRFxxx_CMD_RTA							0x23
#define NRFxxx_CMD_RRP							0x24
#define NRFxxx_CMD_CC(unPwrChn)					((unPwrChn) | 0x8000)
#define CH_MSK_IN_CC_REG						0x01FF
#define NRFxxx_DR_IN_STATUS_REG(status)			((status) & (0x01 << 5))
#define NRFxxx_CMD_READ_STATUS					NRFxxx_CMD_RC(1)
#else
#define NRFxxx_DR_PIN_ISR_EDGE					INT_EDGE_FALLING
#define NRFxxx_TRX_CE_PIN						2
#define NRFxxx_DR_PIN							4
#define NRFxxx_TX_ADDRESS_IN_CR					0x10
#define NRFxxx_RX_ADDRESS_IN_CR					0x0A
#define NRFxxx_RF_CH_ADDR_IN_CR					0x05
#define NRFxxx_STATUS_ADDR_IN_CR				0x07
#define NRFxxx_CMD_WC_MASK						0x1F
#define NRFxxx_CMD_WC(unWR_CFG_ByteIndex)		(((unWR_CFG_ByteIndex) & NRFxxx_CMD_WC_MASK) | 0x20)	// Write Configuration register
#define NRFxxx_CMD_RC_MASK						0x1F
#define NRFxxx_CMD_RC(unRD_CFG_ByteIndex)		((unRD_CFG_ByteIndex) & NRFxxx_CMD_RC_MASK)
#define NRFxxx_CMD_WTP							0xA0
#define NRFxxx_CMD_WAP							0xA8
#define NRFxxx_CMD_RRP							0x61
#define NRFxxx_CMD_RRPW							0x60
#define NRFxxx_CMD_FLUSH_RX_FIFO				0xE2	// flush RX FIFO
#define NRFxxx_CMD_FLUSH_TX_FIFO				0xE1	// flush TX FIFO
#define NRFxxx_DR_IN_STATUS_REG(status)			((status) & (0x01 << 6))
#define NRFxxx_CMD_READ_STATUS					0xFF
#endif

#define NRFxxxSTATUS_LOCK						111

typedef enum _nRFxxxModes {
	NRFxxx_MODE_PWR_DOWN = 0,
	NRFxxx_MODE_STD_BY,
	NRFxxx_MODE_BURST_RX,
	NRFxxx_MODE_BURST_TX,
	NRFxxx_MODE_MAX
} nRFxxxMode_t;

#ifdef NRF905_AS_RF
typedef struct _nRFxxxPinLevelInMode {
	int nPWR_UP_PIN;
	int nTRX_CE_PIN;
	int nTX_EN_PIN;
} nRFxxxPinLevelInMode_t;
#else
typedef struct _nRFxxxPinLevelInMode {
	int nTRX_CE_PIN;
} nRFxxxPinLevelInMode_t;
#endif

// Pin status according to each nRFxxx mode
#ifdef NRF905_AS_RF
static const nRFxxxPinLevelInMode_t unNRFxxxMODE_PIN_LEVEL[] = { { LOW, LOW, LOW },
		{ HIGH, LOW, LOW }, { HIGH, HIGH, LOW }, { HIGH, HIGH, HIGH } };
static const unsigned short int unCAR_REMOTE_HOPPING_TAB[] = { 0x804C, 0x803A, 0x8046, 0x8032, 0x804A, 0x8035,
		0x804B, 0x8037, 0x804F, 0x803E, 0x8047, 0x8038, 0x8044, 0x8034, 0x8043, 0x8034, 0x804B,
		0x8039, 0x804D, 0x803A, 0x804E, 0x803C, 0x8032, 0x803F };
// MSB of CH_NO will always be 0
static const unsigned char NRFxxx_CR_DEFAULT[] = { 0x4C, 0x0C, // F=(422.4+(0x6C<<1)/10)*1; No retransmission; +6db; NOT reduce receive power
		(NRFxxx_RX_ADDR_LEN << 4) | NRFxxx_TX_ADDR_LEN,	// 4 bytes RX & TX address;
		NRFxxx_RX_PAYLOAD_LEN, NRFxxx_TX_PAYLOAD_LEN, // 16 bytes RX & TX package length;
		0x00, 0x0C, 0x40, 0x08,	// RX address is the calculation result of CH_NO
		0x58 };	// 16MHz crystal; enable CRC; CRC16
#else
static const nRFxxxPinLevelInMode_t unNRFxxxMODE_PIN_LEVEL[] = { { LOW },
	{ LOW }, { HIGH }, { HIGH }};
//static const unsigned char unNRFxxxMODE_REG[] = {
//		0x3C, 0x3E, 0x3F, 0x3E
//};
static const unsigned char unCAR_REMOTE_HOPPING_TAB[] = { 0x10, 0x20, 0x12, 0x30, 0x14, 0x40,
		0x16, 0x50, 0x18, 0x22, 0x1A, 0x32, 0x1C, 0x34, 0x24, 0x36, 0x26,
		0x38, 0x28, 0x3A, 0x2A, 0x3C, 0x42, 0x46 };

typedef struct _nRFxxxInitCR {
	unsigned char unCRAddress;
	unsigned char unCRValues;
}nRFxxxInitCR_t;
//static const nRFxxxInitCR_t NRFxxx_CR_DEFAULT[] = {{1, 0x01},
//		{2, 0x01}, {5, 40}, {17, 32}, {6, 0x0F}, {0, 0x3F}};
static const nRFxxxInitCR_t NRFxxx_CR_DEFAULT[] = {{0, 0x3F}, {1, 0x01},
		{2, 0x01}, {3, 0x02}, {4, 0x24}, {5, 40}, {6, 0x08}, {7, 0x70},
		{28, 0x01}, {29, 0x06}};
#endif

typedef struct _nRFxxxStatus {
	unsigned int unNRFxxxRecvFrameCNT;
	unsigned int unNRFxxxSendFrameCNT;
	unsigned int unNRFxxxHoppingCNT;
	unsigned int unNRFxxxTxAddr;
	unsigned int unNRFxxxRxAddr;
#ifdef NRF905_AS_RF
	unsigned short int unNRFxxxCHN_PWR;
#else
	unsigned char unNRFxxxCHN;
#endif
	nRFxxxMode_t tNRFxxxCurrentMode;
}nRFxxxStatus_t;

static nRFxxxStatus_t tNRFxxxStatus = {0, 0, 0, 0, 0, NRFxxx_MODE_PWR_DOWN};

static int nRFxxxSPI_CHN = 0;
static int nRFxxxPipeFd[2];
static unsigned char unNRFxxxPower;

static int setNRFxxxMode(nRFxxxMode_t tNRFxxxMode);

// No set mode in low level APIs because if you set Standby mode
// then after write SPI you don't know what to change back
static int nRFxxxSPI_WR_CMD(unsigned char unCMD, const unsigned char *pData, int nDataLen) {
	int nResult;
	nRFxxxMode_t tPreMode;
	unsigned char* pBuff;
	if (nDataLen >= 0) {
		pBuff = malloc(nDataLen + 1);
		if (pBuff) {
			pBuff[0] = unCMD;
			if (nDataLen > 0) {
				if (pData == NULL) {
					free(pBuff);
					NRFxxx_LOG_ERR("Payload length not zero but no where to copy.");
					return (-1);
				} else {
					memcpy(pBuff + 1, pData, nDataLen);
				}
			}
			tPreMode = tNRFxxxStatus.tNRFxxxCurrentMode;
			setNRFxxxMode(NRFxxx_MODE_STD_BY);
			nResult = wiringPiSPIDataRW(nRFxxxSPI_CHN, pBuff, nDataLen + 1);
			setNRFxxxMode(tPreMode);
			free(pBuff);
//			printf("wiringPiSPIDataRW nResult: %d\n", nResult);
			return nResult;
		} else {
			NRFxxx_LOG_ERR("No free heap.");
			return (-1);
		}
	} else {
		NRFxxx_LOG_ERR("SPI write data length error.");
		return (-1);
	}
}

static int readStatusReg(void) {
	int nResult;
	nRFxxxMode_t tPreMode;
	unsigned char unStatus;
	unStatus = NRFxxx_CMD_READ_STATUS;
	tPreMode = tNRFxxxStatus.tNRFxxxCurrentMode;
	setNRFxxxMode(NRFxxx_MODE_STD_BY);
	nResult = wiringPiSPIDataRW(nRFxxxSPI_CHN, &unStatus, 1);
	setNRFxxxMode(tPreMode);
	if (0 < nResult) {
		return unStatus;
	} else {
		NRFxxx_LOG_ERR("wiringPiSPIDataRW error in readStatusReg with code: %d.", nResult);
		return (-1);
	}
}

static int readRxPayload(unsigned char* pBuff, int nBuffLen) {
	int nResult;
	unsigned char unReadBuff[33];
	nRFxxxMode_t tPreMode;
	if ((nBuffLen > 0) && (nBuffLen < sizeof(unReadBuff))) {
		unReadBuff[0] = NRFxxx_CMD_RRP;
		tPreMode = tNRFxxxStatus.tNRFxxxCurrentMode;
		setNRFxxxMode(NRFxxx_MODE_STD_BY);
		nResult = wiringPiSPIDataRW(nRFxxxSPI_CHN, unReadBuff, nBuffLen + 1);
		setNRFxxxMode(tPreMode);
		memcpy(pBuff, unReadBuff + 1, nBuffLen);
		return nResult;
	} else {
		return (-1);
	}
}

int readConfig(unsigned char unConfigAddr, unsigned char* pBuff, int nBuffLen) {
	int nResult;
	nRFxxxMode_t tPreMode;
	unsigned char unReadBuff[33];

	if ((nBuffLen > 0) && (nBuffLen < sizeof(unReadBuff))) {
		unReadBuff[0] = NRFxxx_CMD_RC(unConfigAddr);
		tPreMode = tNRFxxxStatus.tNRFxxxCurrentMode;
		setNRFxxxMode(NRFxxx_MODE_STD_BY);
		nResult = wiringPiSPIDataRW(nRFxxxSPI_CHN, unReadBuff, nBuffLen + 1);
		setNRFxxxMode(tPreMode);
		memcpy(pBuff, unReadBuff + 1, nBuffLen);
		return nResult;
	} else {
		return (-1);
	}
}

static int writeConfig(unsigned char unConfigAddr, const unsigned char* pBuff, int nBuffLen) {
	return nRFxxxSPI_WR_CMD(NRFxxx_CMD_WC(unConfigAddr), pBuff, nBuffLen);
}

static int writeTxAddr(unsigned int unTxAddr) {
#ifdef NRF905_AS_RF
	return nRFxxxSPI_WR_CMD(NRFxxx_CMD_WTA, (unsigned char*)(&unTxAddr), sizeof(unTxAddr));
#else
	return writeConfig(NRFxxx_TX_ADDRESS_IN_CR, (unsigned char*)(&unTxAddr), sizeof(unTxAddr));
#endif
}

static int writeRxAddr(unsigned int unRxAddr) {
#ifdef NRF905_AS_RF
	return writeConfig(NRFxxx_RX_ADDRESS_IN_CR, (unsigned char*)(&unRxAddr), sizeof(unRxAddr));
#else
	return writeConfig(NRFxxx_RX_ADDRESS_IN_CR, (unsigned char*)(&unRxAddr), sizeof(unRxAddr));
#endif
}

static int setNRFxxxMode(nRFxxxMode_t tNRFxxxMode) {
	if (tNRFxxxMode >= NRFxxx_MODE_MAX){
		NRFxxx_LOG_ERR("nRFxxx Mode error.");
		return (-1);
	}
	piLock(NRFxxxSTATUS_LOCK);
	if (tNRFxxxMode == tNRFxxxStatus.tNRFxxxCurrentMode){
		piUnlock(NRFxxxSTATUS_LOCK);
//		NRFxxx_LOG_INFO("nRFxxx Mode not changed, no need to set pin.");
		return 0;
	}
	piUnlock(NRFxxxSTATUS_LOCK);
//	printf("Set nRFxxx mode to %d.\n", tNRFxxxMode);

#ifdef NRF905_AS_RF
	digitalWrite(NRFxxx_PWR_UP_PIN, unNRFxxxMODE_PIN_LEVEL[tNRFxxxMode].nPWR_UP_PIN);
	digitalWrite(NRFxxx_TX_EN_PIN, unNRFxxxMODE_PIN_LEVEL[tNRFxxxMode].nTX_EN_PIN);
#endif
	digitalWrite(NRFxxx_TRX_CE_PIN, unNRFxxxMODE_PIN_LEVEL[tNRFxxxMode].nTRX_CE_PIN);
//#else
//	printf("Set nRF24L01+ mode to %d.\n", tNRFxxxMode);
//	setNRF24L01PModeInReg(tNRFxxxMode);
//	digitalWrite(NRFxxx_TRX_CE_PIN, unNRFxxxMODE_PIN_LEVEL[tNRFxxxMode].nTRX_CE_PIN);
//#endif
	piLock(NRFxxxSTATUS_LOCK);
	tNRFxxxStatus.tNRFxxxCurrentMode = tNRFxxxMode;
	piUnlock(NRFxxxSTATUS_LOCK);

	return 0;
}

static int setChannelMonitorTimer(int nSeconds) {
	struct itimerval tChannelMonitorTimer;  /* for setting itimer */
	tChannelMonitorTimer.it_value.tv_sec = nSeconds;
	tChannelMonitorTimer.it_value.tv_usec = 0;
	tChannelMonitorTimer.it_interval.tv_sec = nSeconds;
	tChannelMonitorTimer.it_interval.tv_usec = 0;
	return setitimer(ITIMER_REAL, &tChannelMonitorTimer, NULL);
}

#ifdef NRF905_AS_RF
// TX and RX address are already configured during hopping
// No need for nRF24L01+ because it will never send out frame initiatively
// nRF24L01+ only response received package in ACK
static int writeTxPayload(unsigned char* pBuff, int nBuffLen) {
//	printf("writeTxPayload\n");
	return nRFxxxSPI_WR_CMD(NRFxxx_CMD_WTP, pBuff, nBuffLen);
}
static int writeFastConfig(unsigned short int unPA_PLL_CHN) {
	int nResult;
	nRFxxxMode_t tPreMode;
	tPreMode = tNRFxxxStatus.tNRFxxxCurrentMode;
	setNRFxxxMode(NRFxxx_MODE_STD_BY);
	nResult = wiringPiSPIDataRW(nRFxxxSPI_CHN, (unsigned char *)(&unPA_PLL_CHN), 2);
	setNRFxxxMode(tPreMode);
	return nResult;
}

static void dataReadyHandler(void) {
	static unsigned char unReadBuff[NRFxxx_RX_PAYLOAD_LEN];
	static int nStatusReg;
	static int nRxPayloadWidth;

	piLock(NRFxxxSTATUS_LOCK);
	if (NRFxxx_MODE_BURST_RX == tNRFxxxStatus.tNRFxxxCurrentMode) {
		piUnlock(NRFxxxSTATUS_LOCK);
		setNRFxxxMode(NRFxxx_MODE_STD_BY);
		// make sure DR was set
		nStatusReg = readStatusReg();
		if ((nStatusReg >= 0) && (NRFxxx_DR_IN_STATUS_REG(nStatusReg) == 0)) {
			// Strange happens, do something?
			NRFxxx_LOG_ERR("Strange happens. DR pin set but status register not.");
		} else {
			// reset monitor timer since communication seems OK
			setChannelMonitorTimer(1);
			piLock(NRFxxxSTATUS_LOCK);
			tNRFxxxStatus.unNRFxxxRecvFrameCNT++;
			piUnlock(NRFxxxSTATUS_LOCK);
			readRxPayload(unReadBuff, sizeof(unReadBuff));
			if (write(nRFxxxPipeFd[1], unReadBuff, sizeof(unReadBuff)) != sizeof(unReadBuff)) {
				NRFxxx_LOG_ERR("Write nRFxxx pipe error");
			}
		}
		// Response shall be sent before continue receiving
		// For nRF905 after receiving some message, manually response will be sent,
		// After manually response message sent out, nRF905 will be put in RX mode
		// For nRF24F01+, this is done by auto acknowledge
	} else if (NRFxxx_MODE_BURST_TX == tNRFxxxStatus.tNRFxxxCurrentMode) {
		piUnlock(NRFxxxSTATUS_LOCK);
		NRFxxx_LOG_ERR("DR set during TX, switch to receive mode.\n");
		setNRFxxxMode(NRFxxx_MODE_BURST_RX);
	} else {
		piUnlock(NRFxxxSTATUS_LOCK);
		NRFxxx_LOG_ERR("Data ready pin was set but status is neither TX nor RX.");
		setNRFxxxMode(NRFxxx_MODE_BURST_RX);
	}
}
#else
static int readRxPayloadWidth(void) {
	unsigned char unReadBuff[2];
	nRFxxxMode_t tPreMode;

	unReadBuff[0] = NRFxxx_CMD_RRPW;
	tPreMode = tNRFxxxStatus.tNRFxxxCurrentMode;
	setNRFxxxMode(NRFxxx_MODE_STD_BY);
	if (wiringPiSPIDataRW(nRFxxxSPI_CHN, unReadBuff, sizeof(unReadBuff)) > 0) {
		setNRFxxxMode(tPreMode);
		return unReadBuff[1];
	} else {
		setNRFxxxMode(tPreMode);
		return (-1);
	}
}

static int clearDRFlag(void) {
	static const unsigned char unClearDRFlag = 0x50;
	return writeConfig(NRFxxx_STATUS_ADDR_IN_CR, &unClearDRFlag, 1);
}

static void dataReadyHandler(void) {
	static unsigned char unReadBuff[NRFxxx_RX_PAYLOAD_LEN];
	static int nStatusReg;
	static int nRxPayloadWidth;
//	nStatusReg = readStatusReg();
//	printf("DR set: %d!\n", nStatusReg);
	piLock(NRFxxxSTATUS_LOCK);
	if (NRFxxx_MODE_BURST_RX == tNRFxxxStatus.tNRFxxxCurrentMode) {
		piUnlock(NRFxxxSTATUS_LOCK);
		setNRFxxxMode(NRFxxx_MODE_STD_BY);
		// make sure DR was set
		nStatusReg = readStatusReg();
		printf("DR set during RX with status 0x%02X.\n", nStatusReg);
		if ((nStatusReg >= 0) && (NRFxxx_DR_IN_STATUS_REG(nStatusReg) == 0)) {
			// Strange happens, do something?
			NRFxxx_LOG_ERR("Strange happens. DR pin set but status register not.");
		} else {
//			printf("Data ready rising edge detected.\n");
			// reset monitor timer since communication seems OK
			nRxPayloadWidth = readRxPayloadWidth();
			if ((nRxPayloadWidth > 0) && (nRxPayloadWidth <= 32)) {
				setChannelMonitorTimer(1);
				piLock(NRFxxxSTATUS_LOCK);
				tNRFxxxStatus.unNRFxxxRecvFrameCNT++;
				piUnlock(NRFxxxSTATUS_LOCK);
	//			printf("Read RX payload.\n");
				readRxPayload(unReadBuff, sizeof(unReadBuff));

				printf("New frame received: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X.\n",
						unReadBuff[0], unReadBuff[1], unReadBuff[2], unReadBuff[3], unReadBuff[4]);

				if (write(nRFxxxPipeFd[1], unReadBuff, sizeof(unReadBuff)) != sizeof(unReadBuff)) {
					NRFxxx_LOG_ERR("Write nRFxxx pipe error");
				}
			} else {
				NRFxxx_LOG_ERR("RX payload width error!");
			}
		}
	} else if (NRFxxx_MODE_BURST_TX == tNRFxxxStatus.tNRFxxxCurrentMode) {
		piUnlock(NRFxxxSTATUS_LOCK);
		NRFxxx_LOG_ERR("DR set during TX, switch to receive mode.\n");
	} else {
		piUnlock(NRFxxxSTATUS_LOCK);
		NRFxxx_LOG_ERR("Data ready pin was set but status is neither TX nor RX.");
	}
	nRFxxxSPI_WR_CMD(NRFxxx_CMD_FLUSH_RX_FIFO, NULL, 0);
	clearDRFlag();
	// Response shall be sent before continue receiving
	// For nRF905 after receiving some message, manually response will be sent,
	// After manually response message sent out, nRF905 will be put in RX mode
	// For nRF24F01+, this is done by auto acknowledge
	setNRFxxxMode(NRFxxx_MODE_BURST_RX);
}
#endif

static int regDR_Event(void) {
	return wiringPiISR (NRFxxx_DR_PIN, NRFxxx_DR_PIN_ISR_EDGE, &dataReadyHandler) ;
}

static int nRFxxxCRInitial(int nRFxxxSPI_Fd) {
#ifdef NRF905_AS_RF
	return writeConfig(0, NRFxxx_CR_DEFAULT, sizeof(NRFxxx_CR_DEFAULT));
#else
	int i;
//	writeTxAddr(0x12345678);
//	writeRxAddr(0x12345678);
	for (i = 0; i < GET_LENGTH_OF_ARRAY(NRFxxx_CR_DEFAULT); i++) {
		printf("Write CR initial %u value.\n", NRFxxx_CR_DEFAULT[i].unCRValues);
		if (writeConfig(NRFxxx_CR_DEFAULT[i].unCRAddress,
				&(NRFxxx_CR_DEFAULT[i].unCRValues), 1) < 0) {
			NRFxxx_LOG_ERR("Write initial control register to nRF24L01+ error.");
			return (-1);
		}
	}
	return 0;
#endif
}

#ifdef NRF905_AS_RF
#define GET_CHN_PWR_FAST_CONFIG(x, y) 		((x) | ((y) << 10))
static unsigned int getTxAddrFromChnPwr(unsigned int unNRFxxxCHN_PWR) {
	return ((unNRFxxxCHN_PWR | (unNRFxxxCHN_PWR << 16)) & 0xA33D59AA);
}
static unsigned int getRxAddrFromChnPwr(unsigned short int unNRFxxxCHN_PWR) {
	return ((unNRFxxxCHN_PWR | (unNRFxxxCHN_PWR << 16)) & 0x5CA259AA);
}
#else
static unsigned int getRxAddrFromChnPwr(unsigned short int unNRFxxxCHN_PWR) {
	return ((unNRFxxxCHN_PWR | (unNRFxxxCHN_PWR << 8) | (unNRFxxxCHN_PWR << 16) |
			(unNRFxxxCHN_PWR << 24)) & 0x5CA259AA);
}
#endif



static void roamNRFxxx(void) {
	static int nHoppingPoint = 0;

#ifdef NRF905_AS_RF
	setNRFxxxMode(NRFxxx_MODE_STD_BY);
#endif
	piLock(NRFxxxSTATUS_LOCK);
#ifdef NRF905_AS_RF
	tNRFxxxStatus.unNRFxxxCHN_PWR = GET_CHN_PWR_FAST_CONFIG(unCAR_REMOTE_HOPPING_TAB[nHoppingPoint], unNRFxxxPower);
	tNRFxxxStatus.unNRFxxxTxAddr = getTxAddrFromChnPwr(tNRFxxxStatus.unNRFxxxCHN_PWR);
	tNRFxxxStatus.unNRFxxxRxAddr = getRxAddrFromChnPwr(tNRFxxxStatus.unNRFxxxCHN_PWR);
#else
	tNRFxxxStatus.unNRFxxxCHN = unCAR_REMOTE_HOPPING_TAB[nHoppingPoint];
	tNRFxxxStatus.unNRFxxxTxAddr = getRxAddrFromChnPwr(tNRFxxxStatus.unNRFxxxCHN);
	tNRFxxxStatus.unNRFxxxRxAddr = getRxAddrFromChnPwr(tNRFxxxStatus.unNRFxxxCHN);
#endif
	printf("Start hopping with CHN: 0x%02X, RX addr: 0x%08X.\n", tNRFxxxStatus.unNRFxxxCHN, tNRFxxxStatus.unNRFxxxRxAddr);
	(nHoppingPoint < (GET_LENGTH_OF_ARRAY(unCAR_REMOTE_HOPPING_TAB) - 1)) ? (nHoppingPoint++):(nHoppingPoint = 0);
	tNRFxxxStatus.unNRFxxxHoppingCNT++;

	piUnlock(NRFxxxSTATUS_LOCK);
#ifdef NRF905_AS_RF
	writeFastConfig(tNRFxxxStatus.unNRFxxxCHN_PWR);
	writeTxAddr(tNRFxxxStatus.unNRFxxxTxAddr);
#else
	writeConfig(NRFxxx_RF_CH_ADDR_IN_CR, &(tNRFxxxStatus.unNRFxxxCHN), sizeof(tNRFxxxStatus.unNRFxxxCHN));
	writeTxAddr(tNRFxxxStatus.unNRFxxxTxAddr);
#endif
	writeRxAddr(tNRFxxxStatus.unNRFxxxRxAddr);
#ifdef NRF905_AS_RF
	setNRFxxxMode(NRFxxx_MODE_BURST_RX);
#endif
}

int nRFxxxInitial(int nSPI_Channel, int nSPI_Speed, unsigned char unPower) {
	int nRFxxxSPI_Fd;
	unsigned char unCRValue[10];

	printf("nRFxxxInitial.\n");
	wiringPiSetup();
//	printf("Set piHiPri.\n");
	(void)piHiPri(10);

	unNRFxxxPower = unPower;
//	printf("Set pinMode.\n");
#ifdef NRF905_AS_RF
	pinMode(NRFxxx_TX_EN_PIN, OUTPUT);
	pinMode(NRFxxx_PWR_UP_PIN, OUTPUT);
#endif
	pinMode(NRFxxx_TRX_CE_PIN, OUTPUT);
	pinMode(NRFxxx_DR_PIN, INPUT);
	nRFxxxSPI_Fd = wiringPiSPISetup(nSPI_Channel, nSPI_Speed);
	if (nRFxxxSPI_Fd < 0) {
		NRFxxx_LOG_ERR("nRFxxx SPI initial error.");
		return (-1);
	}
	nRFxxxSPI_CHN = nSPI_Channel;
	setNRFxxxMode(NRFxxx_MODE_STD_BY);
	usleep(3000);

#ifdef NRF24L01P_AS_RF
	nRFxxxSPI_WR_CMD(NRFxxx_CMD_FLUSH_TX_FIFO, NULL, 0);
	nRFxxxSPI_WR_CMD(NRFxxx_CMD_FLUSH_RX_FIFO, NULL, 0);
#endif
	printf("nRFxxxCRInitial.\n");
	nRFxxxCRInitial(nRFxxxSPI_Fd);
//	printf("readConfig.\n");
#ifdef NRF905_AS_RF
	readConfig(0, unCRValue, sizeof(unCRValue));
#else
	int i;
	for (i = 0; i < sizeof(unCRValue); i++) {
		readConfig(i, unCRValue + i, 1);
	}
#endif
	printf("CR value read is: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
			"0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			unCRValue[0], unCRValue[1], unCRValue[2], unCRValue[3], unCRValue[4],
			unCRValue[5], unCRValue[6], unCRValue[7], unCRValue[8], unCRValue[9]);

	return 0;
}

int nRFxxxStartListen(void) {
	// generate pip
	if (nRFxxxPipeFd[0] != 0) {
		close(nRFxxxPipeFd[0]);
	}
	if (nRFxxxPipeFd[1] != 0) {
		close(nRFxxxPipeFd[1]);
	}
	if (pipe(nRFxxxPipeFd) != 0) {
		NRFxxx_LOG_ERR("nRFxxx pipe initial error.");
		return (-1);
	}

	// For test only, fix channel, TX and RX address, no hopping
//	writeTxAddr(TEST_NRFxxx_TX_ADDR);
//	writeRxAddr(TEST_NRFxxx_RX_ADDR);

	// Set nRFxxx in RX mode
	setNRFxxxMode(NRFxxx_MODE_BURST_RX);
	// register ISR to handle data receive when DR rise edge
	regDR_Event();

	printf("Start registering SIGALM.\n");
	// start timer to watch communication, if do RX during 1s, start hopping
	if (signal(SIGALRM, (void (*)(int)) roamNRFxxx) == SIG_ERR) {
		NRFxxx_LOG_ERR("Unable to catch SIGALRM");
		close(nRFxxxPipeFd[0]);
		close(nRFxxxPipeFd[1]);
		return (-1);
	}
	raise(SIGALRM);

	if (setChannelMonitorTimer(1) != 0) {
		NRFxxx_LOG_ERR("error calling setitimer()");
		close(nRFxxxPipeFd[0]);
		close(nRFxxxPipeFd[1]);
		return (-1);
	}

	return 0;
}

void setNRFxxxPower(unsigned char unPower) {
	unNRFxxxPower = unPower;
}

// Block operation until there is any data in the pipe which was written in Data ready ISR
int nRFxxxReadFrame(unsigned char* pReadBuff, int nBuffLen) {
	return read(nRFxxxPipeFd[0], pReadBuff, nBuffLen);
}

int nRFxxxSendFrame(void* pReadBuff, int nBuffLen) {
#ifdef NRF905_AS_RF
//	printf("writeTxPayload\n");
	writeTxPayload(pReadBuff, nBuffLen);
//	printf("setNRFxxxMode to TX \n");
	setNRFxxxMode(NRFxxx_MODE_BURST_TX);
#else
//	nRFxxxSPI_WR_CMD(NRFxxx_CMD_FLUSH_TX_FIFO, NULL, 0);
	nRFxxxSPI_WR_CMD(NRFxxx_CMD_WAP, pReadBuff, nBuffLen);
	setNRFxxxMode(NRFxxx_MODE_BURST_RX);
#endif

	// TODO: Better to add timeout here in case DR will not be set.
	// My main task is to receive!
	piLock(NRFxxxSTATUS_LOCK);
	tNRFxxxStatus.unNRFxxxSendFrameCNT++;
	piUnlock(NRFxxxSTATUS_LOCK);
	return 0;
}

int nRFxxxStopListen(void) {
	setNRFxxxMode(NRFxxx_MODE_STD_BY);
	close(nRFxxxPipeFd[0]);
	close(nRFxxxPipeFd[1]);
	return 0;
}

unsigned int getNRFxxxStatusRecvFrameCNT(void) {
	unsigned int unNRFxxxRecvFrameCNT;
	piLock(NRFxxxSTATUS_LOCK);
	unNRFxxxRecvFrameCNT = tNRFxxxStatus.unNRFxxxRecvFrameCNT;
	piUnlock(NRFxxxSTATUS_LOCK);
	return unNRFxxxRecvFrameCNT;
}

unsigned int getNRFxxxStatusSendFrameCNT(void) {
	unsigned int unNRFxxxSendFrameCNT;
	piLock(NRFxxxSTATUS_LOCK);
	unNRFxxxSendFrameCNT = tNRFxxxStatus.unNRFxxxSendFrameCNT;
	piUnlock(NRFxxxSTATUS_LOCK);
	return unNRFxxxSendFrameCNT;
}

unsigned int getNRFxxxStatusHoppingCNT(void) {
	unsigned int unNRFxxxHoppingCNT;
	piLock(NRFxxxSTATUS_LOCK);
	unNRFxxxHoppingCNT = tNRFxxxStatus.unNRFxxxHoppingCNT;
	piUnlock(NRFxxxSTATUS_LOCK);
	return unNRFxxxHoppingCNT;
}
