#include <ucx.h>

void task2(void)
{
	int32_t cnt = 300000;
	volatile int32_t val;

	ucx_task_init();

	while (1) {
		val = kcb_p->ctx_switches;
		if (kcb_p->ctx_switches % 5 == 0) {
			struct tcb_s *original_tcb = kcb_p->tcb_p;
			struct tcb_s *tcb = kcb_p->tcb_p;
			do {
				if (tcb->period > 0)
					printf("task %d -> periodo restante: %d, capacidade restante: %d, perdas de deadline: %d\n", tcb->id, tcb->remaining_period, tcb->remaining_capacity, tcb->deadline_misses); 
				tcb = tcb->tcb_next;
			} while (tcb != original_tcb);
		}
		printf("[task %d %ld]\n", ucx_task_id(), cnt++);
		while (kcb_p->ctx_switches == val);
	}
}

void task1(void)
{
	int32_t cnt = 200000;
	volatile int32_t val;

	ucx_task_init();

	while (1) {
		val = kcb_p->ctx_switches;
		printf("[task %d %ld]\n", ucx_task_id(), cnt++);
		while (kcb_p->ctx_switches == val);
	}
}

void task0(void)
{
	int32_t cnt = 100000;
	volatile int32_t val;

	ucx_task_init();

	while (1) {
		val = kcb_p->ctx_switches;
		printf("[task %d %ld]\n", ucx_task_id(), cnt++);
		while (kcb_p->ctx_switches == val);
	}
}

int32_t app_main(void)
{
	ucx_task_add(task0, DEFAULT_GUARD_SIZE, 4, 12);
	ucx_task_add(task1, DEFAULT_GUARD_SIZE, 2, 4);
	ucx_task_add(task2, DEFAULT_GUARD_SIZE, 0, 0);
	
	ucx_task_priority(2, TASK_LOW_PRIO);

	// start UCX/OS, preemptive mode
	return 1;
}
