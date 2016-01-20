#include "SCHEDULER.h"

#include <stdio.h>
#include <stdlib.h>

int thread_1(void *arg)
{
	printf("Hello from thread 1 arg is a pointer to 0x%08x!\r\n", arg);

	SCHEDULER__yield();

	printf("Hello from thread 1 again!\r\n");

	return 0;
}

int thread_2(void *arg)
{
	printf("Hello from thread 2! arg is a pointer to 0x%08x\r\n", arg);

	SCHEDULER__yield();

	printf("Hello from thread 2 again!\r\n");

	return 0;
}

int thread_3(void *arg)
{
	printf("Hello from thread 3! arg is a pointer to 0x%08x\r\n", arg);

	SCHEDULER__yield();

	printf("Hello from thread 3 again!\r\n");

	return 0;
}

int thread_1_arg = 0;
int thread_2_arg = 1;
int thread_3_arg = 1337;

struct {
	THREAD__entry_point_t *entry_point;
	void *arg;
} threads[] = {
	{thread_1, &thread_1_arg},
	{thread_2, &thread_2_arg},
	{thread_3, &thread_3_arg},
};

int main(void)
{
	size_t i = 0;

	SCHEDULER__init();

	/* Add all threads to the scheduler. */
	for (i = 0; i < _countof(threads); ++i) {
		SCHEDULER__add_thread(threads[i].entry_point, 
			                  threads[i].arg);
	}

	SCHEDULER__schedule_threads();
	return 0;
}