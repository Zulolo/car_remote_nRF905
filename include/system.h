#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <syslog.h>

#define REMOTE_CAR_LOG_ERR(arg...)		openlog("remote car.err", LOG_PID, 0);\
										syslog(LOG_USER | LOG_INFO, arg);\
										closelog()

#define REMOTE_CAR_LOG_INFO(arg...)		openlog("remote car.info", LOG_PID, 0);\
										syslog(LOG_USER | LOG_INFO, arg);\
										closelog()

int nSetSystemValue(char* pKey, char* pValue);

#endif
