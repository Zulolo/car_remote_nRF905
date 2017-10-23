
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "unity.h"
#include "system.h"

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

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_nSetSystemValue_performance);
	return UNITY_END();
}
