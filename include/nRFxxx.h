#ifndef NRFXXX_H_
#define NRFXXX_H_

#include <syslog.h>

#define NRFxxx_LOG_ERR(arg...)			openlog("nRFxxx.err", LOG_PID, 0);\
										syslog(LOG_USER | LOG_INFO, arg);\
										closelog()

#define NRFxxx_LOG_INFO(arg...)			openlog("nRFxxx.info", LOG_PID, 0);\
										syslog(LOG_USER | LOG_INFO, arg);\
										closelog()

#define ARRAY_LENGTH(x)					(sizeof(x)/sizeof(x[0]))

#ifdef NRF905_AS_RF
	#define NRFxxx_RX_PAYLOAD_LEN			16
	#define NRFxxx_TX_PAYLOAD_LEN			NRFxxx_RX_PAYLOAD_LEN
#elif NRF24L01P_AS_RF
	#define NRFxxx_RX_PAYLOAD_LEN			32
	#define NRFxxx_TX_PAYLOAD_LEN			NRFxxx_RX_PAYLOAD_LEN
#endif


int nRFxxxInitial(int nSPI_Channel, int nSPI_Speed, unsigned char unPower);
int nRFxxxStartListen(void);
int nRFxxxReadFrame(unsigned char* pReadBuff, int nBuffLen);
int nRFxxxSendFrame(void* pReadBuff, int nBuffLen);
int readConfig(unsigned char unConfigAddr, unsigned char* pBuff, int nBuffLen);
unsigned int getNRFxxxStatusRecvFrameCNT(void);
unsigned int getNRFxxxStatusSendFrameCNT(void);
unsigned int getNRFxxxStatusHoppingCNT(void);

#endif
