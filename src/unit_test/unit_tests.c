
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "unity.h"
#include "system.h"
#include "wiringPi.h"
#include "nRF905.h"
#include "nRF905Handler.h"

PI_THREAD (nRF905Thread)
{
	nRemoteCarStartReceive();
	return NULL ;
}

void test_nSetSystemValue_performance(void){
	int i;
	char keys[40];
	char values[40];
	time_t t_current_time_pre, t_current_time_post;

	t_current_time_pre = time(NULL);
	for (i = 0; i < 10000; i++) {
		sprintf(keys, "remote_car:key_%d", i);
		sprintf(values, "remote_car:value_%d", i);
		TEST_ASSERT_EQUAL_INT(0, nSetSystemValue(keys, values));
	}
	t_current_time_post = time(NULL);
	printf("Average %f ms was used to insert one key-value pair.\n",
			(float)(t_current_time_post - t_current_time_pre)/(float)10000);

}

void test_nrf905_communication(void){
	int i = 0;
	int nRF905ThreadID;
	unsigned char unReadBuff[32];
	nRF905ThreadID = piThreadCreate(nRF905Thread) ;
	if (nRF905ThreadID != 0) {
		printf("nRF905 receive thread start error.\n");
		return;
	}

	while (i < 10) {
		printf("nRF905 receive frame: %d.\n", getNRF905StatusRecvFrameCNT());
		printf("nRF905 send frame: %d.\n", getNRF905StatusSendFrameCNT());
		printf("nRF905 hopping: %d.\n", getNRF905StatusHoppingCNT());
		i++;
		sleep(1);
	}
	readConfig(0, unReadBuff, 10);
	printf("nRF905 config: \n");
	for (i = 0; i < 10; i++) {
		printf("0x%02X \n", unReadBuff[i]);
	}
}

int main(void) {

	UNITY_BEGIN();
//	RUN_TEST(test_nSetSystemValue_performance);

	RUN_TEST(test_nrf905_communication);

	return UNITY_END();
}
