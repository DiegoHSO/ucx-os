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
	// inicio do round robin
	struct tcb_s *tcb = kcb_p->tcb_p;
	
	do {
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
			kcb_p->tcb_p->remaining_period = kcb_p->tcb_p->remaining_period - 1;
		} while (kcb_p->tcb_p->period != -1);
	} while (kcb_p->tcb_p != tcb);

	do {
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		} while (kcb_p->tcb_p->state != TASK_READY || kcb_p->tcb_p->period > 0);
	} while (--kcb_p->tcb_p->priority & 0xff);
	kcb_p->tcb_p->priority |= (kcb_p->tcb_p->priority >> 8) & 0xff;
	// fim do round robin
	kcb_p->tcb_p->state = TASK_RUNNING;
	kcb_p->ctx_switches++;
	
	return kcb_p->tcb_p->id;
}

// task perdeu deadline? ignora o resto das computacoes e segue em frente
uint16_t krnl_rm(void)
{
	kcb_p->tcb_p->remaining_capacity = kcb_p->tcb_p->remaining_capacity - 1;

	// lembrar de descontar o remaining_period de cada tarefa a cada interrupcao
	if (kcb_p->tcb_p->state == TASK_RUNNING)
		kcb_p->tcb_p->state = TASK_READY;

	struct tcb_s *tcb = kcb_p->tcb_p;
	uint16_t lowest_period = 65535;
	uint16_t id = -1;


	printf("consegui chegar aqui em cima\n");
	do {
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		} while (kcb_p->tcb_p->period == -1);
		printf("periodo da proxima tarefa %d\n", kcb_p->tcb_p->period);
		kcb_p->tcb_p->remaining_capacity = kcb_p->tcb_p->remaining_period == 0 ? kcb_p->tcb_p->capacity : kcb_p->tcb_p->remaining_capacity;
		kcb_p->tcb_p->remaining_period = kcb_p->tcb_p->remaining_period == 0 ? kcb_p->tcb_p->period : kcb_p->tcb_p->remaining_period - 1;
	} while (kcb_p->tcb_p != tcb);
	printf("consegui chegar aqui\n");
	do {
		id = kcb_p->tcb_p->id;
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		} while (kcb_p->tcb_p->state != TASK_READY || 
				 kcb_p->tcb_p->period == -1 ||
				 kcb_p->tcb_p->remaining_capacity == 0 || 
				 kcb_p->tcb_p->period > lowest_period);
		printf("achei uma tarefa de menor periodo! o periodo atual eh %d e o periodo da proxima tarefa eh %d, com capacidade %d e periodo restante %d\n", lowest_period, kcb_p->tcb_p->period, kcb_p->tcb_p->remaining_capacity, kcb_p->tcb_p->remaining_period);
		lowest_period = kcb_p->tcb_p->period;
		// id = kcb_p->tcb_p->id;
	} while (kcb_p->tcb_p->id != id);

	// do {
	// 	kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
	// } while (kcb_p->tcb_p->state != TASK_READY || kcb_p->tcb_p->period == -1); // de todas as tarefas que estao prontas
	kcb_p->tcb_p->state = TASK_RUNNING;
	kcb_p->ctx_switches++; // incrementa o numero de trocas de contexto
	
	return id;
		// if (kcb_p->tcb_p->period > 0)
		// 	// chamar escalonador RM
		// else 
}

void krnl_dispatcher(void)
{
	if (!setjmp(kcb_p->tcb_p->context)) {
		krnl_delay_update();
		krnl_guard_check();
		uint16_t id = krnl_rm();
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
