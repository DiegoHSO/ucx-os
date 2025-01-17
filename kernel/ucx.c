/* file:          ucx.c
 * description:   UCX/OS kernel
 * date:          04/2021
 * author:        Sergio Johann Filho <sergio.johann@acad.pucrs.br>
 */

#include <ucx.h>

struct kcb_s kernel_state;
struct kcb_s *kcb_p = &kernel_state;

/* kernel auxiliary functions */

void krnl_guard_check(void)
{
	uint32_t check = 0x33333333;
	
	if (*kcb_p->tcb_p->guard_addr != check) {
		ucx_hexdump((void *)kcb_p->tcb_p->guard_addr, kcb_p->tcb_p->guard_sz);
		printf("\n*** HALT - task %d, guard %08x (%d) check failed\n", kcb_p->tcb_p->id,
			(size_t)kcb_p->tcb_p->guard_addr, (size_t)kcb_p->tcb_p->guard_sz);
		for (;;);
	}
		
}

void krnl_delay_update(void)
{
	struct tcb_s *tcb_ptr = kcb_p->tcb_first;
	
	for (;;	tcb_ptr = tcb_ptr->tcb_next) {
		if (tcb_ptr->state == TASK_BLOCKED && tcb_ptr->delay > 0) {
			tcb_ptr->delay--;
			if (tcb_ptr->delay == 0)
				tcb_ptr->state = TASK_READY;
		}
		if (tcb_ptr->tcb_next == kcb_p->tcb_first) break;
	}
}

void krnl_sched_init(int32_t preemptive)
{
	kcb_p->tcb_p = kcb_p->tcb_first;

	if (preemptive) {
		_timer_enable();
	}
	(*kcb_p->tcb_p->task)();
}


/* task scheduler and dispatcher */

uint16_t krnl_schedule(void)
{
	if (kcb_p->tcb_p->state == TASK_RUNNING)
		kcb_p->tcb_p->state = TASK_READY;

	do {
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		} while (kcb_p->tcb_p->state != TASK_READY || kcb_p->tcb_p->period > 0);
	} while (--kcb_p->tcb_p->priority & 0xff);
	kcb_p->tcb_p->priority |= (kcb_p->tcb_p->priority >> 8) & 0xff;
	
	kcb_p->tcb_p->state = TASK_RUNNING;
	kcb_p->ctx_switches++;
	
	return kcb_p->tcb_p->id;
}

int16_t krnl_rm(void)
{
	kcb_p->tcb_p->remaining_capacity = kcb_p->tcb_p->period == 0 ? 0 : kcb_p->tcb_p->remaining_capacity - 1;

	if (kcb_p->tcb_p->state == TASK_RUNNING)
		kcb_p->tcb_p->state = TASK_READY;

	struct tcb_s *tcb = kcb_p->tcb_p;
	uint16_t lowest_period = 65535;
	uint16_t number_of_tasks = 0;
	uint16_t idle_tasks = 0;
	uint16_t id = 0;

	// desconta o periodo restante de cada tarefa, reinicializa as capacidades das tarefas com o novo periodo e verifica se houve perda de deadline
	do {
		number_of_tasks++;
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		kcb_p->tcb_p->deadline_misses = kcb_p->tcb_p->remaining_period == 1 && kcb_p->tcb_p->remaining_capacity > 0 ? kcb_p->tcb_p->deadline_misses + 1 : kcb_p->tcb_p->deadline_misses;
		kcb_p->tcb_p->remaining_capacity = kcb_p->tcb_p->remaining_period == 1 ? kcb_p->tcb_p->capacity : kcb_p->tcb_p->remaining_capacity;
		kcb_p->tcb_p->remaining_period = kcb_p->tcb_p->remaining_period < 2 ? kcb_p->tcb_p->period : kcb_p->tcb_p->remaining_period - 1;
	} while (kcb_p->tcb_p != tcb);

	// conta quantas tarefas nao possuem mais computacoes a serem feitas dentro do seu periodo
	do {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		idle_tasks = kcb_p->tcb_p->remaining_capacity == 0 ? idle_tasks + 1 : idle_tasks;
	} while (kcb_p->tcb_p != tcb);

	// se nao ha tarefas a serem executadas com o RM, retorna -1 para que possa ir por round robin
	if (idle_tasks == number_of_tasks) {
		return -1;
	}

	// se ha tarefas a serem executadas com o RM, retorna o id da tarefa com menor periodo
	do {
		id = kcb_p->tcb_p->id;
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		} while (kcb_p->tcb_p->state != TASK_READY || 
				 kcb_p->tcb_p->period == 0 ||
				 kcb_p->tcb_p->remaining_capacity == 0 || 
				 kcb_p->tcb_p->period > lowest_period);
		lowest_period = kcb_p->tcb_p->period;
	} while (kcb_p->tcb_p->id != id);

	kcb_p->tcb_p->state = TASK_RUNNING;
	kcb_p->ctx_switches++;
	
	return id;
}

void krnl_dispatcher(void)
{
	if (!setjmp(kcb_p->tcb_p->context)) {
		krnl_delay_update();
		krnl_guard_check();

		int16_t id = krnl_rm();
		// caso nao haja tarefas a serem executadas com o RM, segue em frente com o round robin
		if (id < 0)
			krnl_schedule();

		_interrupt_tick();
		longjmp(kcb_p->tcb_p->context, 1);
	}
}

/* main() function, called from the C runtime */

int32_t main(void)
{
	int32_t pr;
	
	_hardware_init();
	
	kcb_p->tcb_p = 0;
	kcb_p->tcb_first = 0;
	kcb_p->ctx_switches = 0;
	kcb_p->id = 0;
	
	printf("UCX/OS boot on %s\n", __ARCH__);
#ifndef UCX_OS_HEAP_SIZE
	ucx_heap_init((size_t *)&_heap_start, (size_t)&_heap_size);
	printf("heap_init(), %d bytes free\n", (size_t)&_heap_size);
#else
	ucx_heap_init((size_t *)&_heap, UCX_OS_HEAP_SIZE);
	printf("heap_init(), %d bytes free\n", UCX_OS_HEAP_SIZE);
#endif
	pr = app_main();
	krnl_sched_init(pr);
	
	return 0;
}
