#include <ucx.h>

void task2(void)
{
	int32_t cnt = 300000;

	ucx_task_init();

	while (1) {
		printf("[task %d %ld]\n", ucx_task_id(), cnt++);
		_delay_ms(1000);
	}
}

void task1(void)
{
	int32_t cnt = 200000;

	ucx_task_init();

	while (1) {
		printf("[task %d %ld]\n", ucx_task_id(), cnt++);
		_delay_ms(1000);
	}
}

void task0(void)
{
	int32_t cnt = 100000;

	ucx_task_init();

	while (1) {
		printf("[task %d %ld]\n", ucx_task_id(), cnt++);
		_delay_ms(1000);
	}
}

int32_t app_main(void)
{
	ucx_task_add(task0, DEFAULT_GUARD_SIZE, 4, 8);
	ucx_task_add(task1, DEFAULT_GUARD_SIZE, 2, 4);
	ucx_task_add(task2, DEFAULT_GUARD_SIZE, 0, 0);
	
	ucx_task_priority(2, TASK_LOW_PRIO);

	// start UCX/OS, preemptive mode
	return 1;
}
