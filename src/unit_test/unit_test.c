
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "system.h"

int main(void) {
	int i;
	char keys[20];
	char values[20];
	time_t t_current_time_pre, t_current_time_post;

	puts("!!!Hello my remote car unit test!!!"); /* prints !!!Hello my remote car!!! */

	t_current_time_pre = time(NULL);
	for (i = 0; i < 10000; i++) {
		sprintf(keys, "key_%d", i);
		sprintf(values, "value_%d", i);
		nSetSystemValue(keys, values);
	}
	t_current_time_post = time(NULL);
	printf("total time used -- %lds.\n", t_current_time_post - t_current_time_pre);
	return EXIT_SUCCESS;
}
