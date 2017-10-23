#ifndef NRF905_H_
#define NRF905_H_

#include <syslog.h>

#define NRF905_LOG_ERR(arg...)			openlog("nRF905.err", LOG_PID, 0);\
										syslog(LOG_USER | LOG_INFO, arg);\
										closelog()

#define NRF905_LOG_INFO(arg...)			openlog("nRF905.info", LOG_PID, 0);\
										syslog(LOG_USER | LOG_INFO, arg);\
										closelog()


int nRF905Initial(int nSPI_Channel, int nSPI_Speed);
int nRF905StartListen(unsigned short int* pHoppingTable, int nTableLen);
int nRF905ReadFrame(unsigned char* pReadBuff, int nBuffLen);
int nRF905SendFrame(unsigned char* pReadBuff, int nBuffLen);

#endif
