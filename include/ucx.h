#include <hal.h>
#include <libc.h>
#include <dump.h>
#include <list.h>
#include <queue.h>
#include <pipe.h>
#include <semaphore.h>
#include <timer.h>
#include <malloc.h>
#include <stdarg.h>

/* task priorities */
#define TASK_CRIT_PRIO		((0x03 << 8) | 0x03)		/* priority 0 .. 3 */
#define TASK_HIGH_PRIO 		((0x0f << 8) | 0x0f)		/* priority 4 .. 15 */
#define TASK_NORMAL_PRIO	((0x1f << 8) | 0x1f)		/* priority 16 .. 31 */
#define TASK_LOW_PRIO		((0x3f << 8) | 0x3f)		/* priority 32 .. 63 */
#define TASK_IDLE_PRIO		((0x7f << 8) | 0x7f)		/* priority 64 .. 127 */

/* task states */
enum {TASK_STOPPED, TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_SUSPENDED};

/* task control block node */
struct tcb_s {
	struct tcb_s *tcb_next;
	void (*task)(void);
	jmp_buf context;
	uint32_t *guard_addr;
	uint16_t guard_sz;
	uint16_t id;
	uint16_t delay;
	uint16_t priority;
	uint8_t state;
	uint16_t period;
	uint16_t remaining_period;
	uint16_t capacity;
	uint16_t remaining_capacity;
	uint16_t deadline_misses;
};

/* kernel control block */
struct kcb_s {
	struct tcb_s *tcb_p;
	struct tcb_s *tcb_first;
	volatile uint32_t ctx_switches;
	uint16_t id;
};

extern struct kcb_s *kcb_p;

/* kernel API */

#define CRITICAL_ENTER		_timer_disable
#define CRITICAL_LEAVE		_timer_enable

void krnl_guard_check(void);
void krnl_delay_update(void);
void krnl_sched_init(int32_t preemptive);
uint16_t krnl_schedule(void);
int16_t krnl_rm(void);
void krnl_dispatcher(void);

int32_t ucx_task_add(void *task, uint16_t guard_size, uint16_t capacity, uint16_t period);
void ucx_task_init();
void ucx_task_yield();
void ucx_task_delay(uint16_t ticks);
int32_t ucx_task_suspend(uint16_t id);
int32_t ucx_task_resume(uint16_t id);
int32_t ucx_task_priority(uint16_t id, uint16_t priority);
uint16_t ucx_task_id();
void ucx_task_wfi();
uint16_t ucx_task_count();
int32_t app_main();