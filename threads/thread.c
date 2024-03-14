#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-point.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* THREAD_READY 상태의 스레드 리스트, 즉 실행할 준비가 되었지만 실제로 실행되지는 않은 스레드의 리스트
 * priority를 기준으로 내림차순 정렬되어 있다.
 */
static struct list ready_list;

/* THREAD_BLOCKED 상태의 스레드 리스트, 즉 실행할 준비가 되지 않은 스레드의 리스트
 * wakeup_tick을 기준으로 오름차순 정렬되어 있다.
 */
static struct list sleep_list;

/* RUNNING, READY, BLOCKED 상태의 모든 스레드 리스트
 * IDLE 스레드는 포함하지 않는다.
 */
static struct list all_list;

/* idel_thread는 다른 스레드가 실행할 준비가 되지 않았을 때 실행되는 스레드이다.
 * CPU가 유휴(Idle) 상태, 즉 아무런 작업을 하지 않을 때 실행된다.
 * idel_thread는 thread_start()에서 생성되고, 우선순위는 0이다.
 */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4		  /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/* load_avg - 시스템의 load_avg * 2^14(F)을 저장한다.
 */
int load_avg;


static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);

void calculate_load_avg(void);
void calculate_all_recent_cpu(void);
int calculate_one_recent_cpu(struct thread *t);
void recent_cpu_plus(void);
void calculate_all_priority(void);
int calculate_one_priority(struct thread *t);

int load_avg;

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */

// 현재 실행중인 스레드의 스택 포인터 'rsp'를 읽어서 해당 스레드의 시작 주소를 반환
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the globla thread context */
	lock_init(&tid_lock);
	list_init(&ready_list);
	list_init(&sleep_list);
	list_init(&all_list);
	list_init(&destruction_req);
	load_avg = 0;

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
	load_avg = 0;
}

/* thread_start - 인터럽트를 활성화하여 프리미티브 스레드 스케줄링을 시작하고, Idle 스레드를 생성한다.
 */
void thread_start(void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down(&idle_started);
}

/* 타이머 인터럽트 핸들러에 의해 각 타이머 틱마다 호출된다.
 * 따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행된다.
 */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* 새로운 커널 스레드를 생성한다. NAME은 스레드의 이름이며, PRIORITY는 스레드의 우선순위이다.
 * FUNCTION은 스레드가 실행할 함수이며, AUX는 FUNCTION에 전달할 인자이다.
 * 스레드를 생성하고, ready_list에 스레드를 추가한다.
 * 새 스레드의 tid를 반환하거나 생성에 실패하면 TID_ERROR를 반환한다.
 * FUNCTION이 실행되는 시점은 thread_create()가 반환된 이후이다.
 * 새로 생성된 스레드와 현재 실행 중인 스레드의 우선 순위를 비교하여 현재 스레드의 우선 순위가 더 낮다면 CPU를 양보한다.

 * thread_start()가 호출되었다면 thread_create()가 반환되기 전에 새 스레드가 예약될 수 있습니다.
 * 심지어 thread_create()가 반환되기 전에 종료될 수도 있습니다.
 * 반대로, 원래 스레드는 새 스레드가 예약되기 전에 얼마든지 실행될 수 있습니다.
 * 순서를 보장해야 하는 경우 세마포어 또는 다른 형태의 동기화를 사용하십시오.
 */
tid_t thread_create(const char *name, int priority, thread_func *function, void *aux)
{
	enum intr_level old_level;
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();


	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock(t);

	old_level = intr_disable();
	if (thread_get_priority() < priority)
		thread_yield();

	intr_set_level(old_level);

	return tid;
}

/* 현재 스레드를 재운다(BLOCKED로 변경). thread_unblock()이 호출될때까지 다시 스케줄되지 않는다.
 * 이 함수는 인터럽트가 꺼진 상태에서 호출되어야 한다.
 * 보통 synch.h에 있는 동기화 프리미티브를 사용하는 것이 더 좋다.
 */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* thread_unblock - BLOCKED 스레드 t를 READY 상태로 전환하고 ready_list에 우선순위 내림차순으로 삽입한다.
 * t가 BLOCKED 상태가 아니라면 에러이다. (running thread를 READY 상태로 전환하려면 thread_yield()를 사용하라)
 * 이 함수는 현재 실행중인 스레드를 선점하지 않는다. 이것은 중요할 수 있다.
 * 만약 호출자가 직접 인터럽트를 비활성화했다면, 스레드를 unblock하고 다른 데이터를 업데이트할 수 있다고 기대할 수 있다.
 */
void thread_unblock(struct thread *t)
{
	enum intr_level old_level;
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_BLOCKED);

	old_level = intr_disable();
	list_insert_ordered(&ready_list, &t->elem, (list_less_func *)higher_priority, NULL);
	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *thread_current(void)
{
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* thread_yield - 현재 실행 중인 스레드가 CPU를 양보하고, ready_list에 우선순위 내림차순으로 삽입한다.
 * 현재 스레드는 BLOCKED 상태로 전환되지 않으며 스케줄러의 재량에 따라 즉시 다시 스케줄될 수 있다.
 * 만약 현재 스레드가 Idle 스레드라면 ready_list에 삽입하지 않는다.
 */
void thread_yield(void)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	ASSERT(!intr_context());

	old_level = intr_disable();
	if (curr != idle_thread)
		list_insert_ordered(&ready_list, &curr->elem, (list_less_func *)higher_priority, NULL);
	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

void thread_try_yield(void) {
	if (!list_empty(&ready_list) && thread_current() != idle_thread && !intr_context())
		thread_yield();
}

/* thread_set_priority - 현재 스레드의 우선순위를 새로운 우선순위로 설정하고,
 * 우선순위가 낮아진다면 ready_list에서 자신보다 더 높은 우선순위를 가진 스레드가 있는지 확인하여야 한다.
 * 고급 스케줄러를 사용하는 경우에는 이 함수를 사용하지 않는다.
 */
void thread_set_priority(int new_priority)
{
	thread_current()->original_priority = new_priority;
	if (list_empty(&thread_current()->donations))
	{
		thread_current()->priority = new_priority;
	}
	if (!list_empty(&ready_list) && list_entry(list_front(&ready_list), struct thread, elem)->priority > new_priority)
		thread_yield();
}

/* thread_get_priority - 현재 스레드의 우선순위를 반환한다.
 */
int thread_get_priority(void)
{
	return thread_current()->priority;
}

/* thread_set_nice - 현재 스레드의 nice 값을 새로운 nice로 설정한다.
 * 새로 설정된 nice 값에 따라 스레드의 우선순위가 변경되고, 우선순위가 변경된 경우 스케줄링을 수행한다.
 */
void thread_set_nice(int nice)
{
	struct thread *t = thread_current();
	t->nice = nice;
	thread_set_priority(calculate_one_priority(t));
}

/* thread_get_nice - 현재 스레드의 nice 값을 반환한다.
 */
int thread_get_nice(void)
{
	return thread_current()->nice;
}

/* thread_get_load_avg - 시스템의 load_avg * 100을 반환한다.
 */
int thread_get_load_avg(void)
{
	return convert_to_integer_towards_nearest(multiply_fixed_point_integer(load_avg, 100));
}

/* thread_get_recent_cpu - 현재 스레드의 recent_cpu * 100을 반환한다.
 */
int thread_get_recent_cpu(void)
{
	return convert_to_integer_towards_zero(multiply_fixed_point_integer(thread_current()->recent_cpu, 100));
}

/* calculate_load_avg - load_avg를 1초마다 계산한다.
 * load_avg = (59/60)*load_avg + (1/60)*ready_threads
 */
void calculate_load_avg(void)
{
	int ready_threads = list_size(&ready_list);
	if (thread_current() != idle_thread)
		ready_threads++;
	load_avg = multiply_fixed_point((59 * F) / 60, load_avg) + (((1 * F) / 60) * ready_threads);
}

/* calculate_all_recent_cpu - 모든 스레드의 recent_cpu를 1초마다 계산한다.
 */
void calculate_all_recent_cpu(void)
{
	if (list_empty(&all_list))
		return;

	struct list_elem *e;
	struct thread *t;
	for (e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e))
	{
		t = list_entry(e, struct thread, a_elem);
		t->recent_cpu = calculate_one_recent_cpu(t);
		t->priority = calculate_one_priority(t);
	}
}

/* calculate_one_recent_cpu - 스레드 t의 recent_cpu를 계산한다.
 * decay = (2 * load_avg) / (2 * load_avg + 1)
 * recent_cpu = decay * recent_cpu + nice
 */
int calculate_one_recent_cpu(struct thread *t)
{
	int decay = divide_fixed_point(multiply_fixed_point_integer(load_avg, 2), add_fixed_point_integer(multiply_fixed_point_integer(load_avg, 2), 1));
	int _recent_cpu = add_fixed_point_integer(multiply_fixed_point(decay, t->recent_cpu), t->nice);
	return _recent_cpu;
}

/* recent_cpu_plus - 현재 스레드의 recent_cpu를 1초마다 1 증가시킨다.
 */
void recent_cpu_plus(void)
{
	struct thread *t = thread_current();
	ASSERT(t->status == THREAD_RUNNING);
	if (t != idle_thread) 
	{
		t->recent_cpu = add_fixed_point_integer(t->recent_cpu, 1);
	}
}

/* calculate_all_priority - 모든 스레드의 priority를 4 ticks마다 계산한다.
 */
void calculate_all_priority(void) 
{
	if (list_empty(&all_list))
		return;

	struct list_elem *e;
	struct thread *t;

	for (e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e))
	{
		t = list_entry(e, struct thread, a_elem);
		t->priority = calculate_one_priority(t);
	}
}
/* calculate_one_priority - 스레드 t의 priority를 계산한다.
 */
int calculate_one_priority(struct thread *t)
{
	int priority = PRI_MAX - convert_to_integer_towards_zero(t->recent_cpu / 4) - (t->nice * 2);
	priority = MAX(priority, PRI_MIN);
	priority = MIN(priority, PRI_MAX);
	return priority;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		intr_disable();
		thread_block();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */
	function(aux); /* Execute the thread function. */
	thread_exit(); /* If function() returns, kill the thread. */
}

/* init_thread - 스레드 t를 name이라는 priority를 가진 BLOCKED 스레드로 초기화한다.
 * donations 리스트를 초기화한다.
 */
static void init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;
	t->original_priority = priority;
	t->nice = 0;
	t->recent_cpu = 0;
	t->magic = THREAD_MAGIC;
	list_init(&t->donations);
	if (strcmp(name, "idle"))
		list_push_back(&all_list, &t->a_elem);
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		return idle_thread;
	else
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"
		"movq 0(%%rsp),%%r15\n"
		"movq 8(%%rsp),%%r14\n"
		"movq 16(%%rsp),%%r13\n"
		"movq 24(%%rsp),%%r12\n"
		"movq 32(%%rsp),%%r11\n"
		"movq 40(%%rsp),%%r10\n"
		"movq 48(%%rsp),%%r9\n"
		"movq 56(%%rsp),%%r8\n"
		"movq 64(%%rsp),%%rsi\n"
		"movq 72(%%rsp),%%rdi\n"
		"movq 80(%%rsp),%%rbp\n"
		"movq 88(%%rsp),%%rdx\n"
		"movq 96(%%rsp),%%rcx\n"
		"movq 104(%%rsp),%%rbx\n"
		"movq 112(%%rsp),%%rax\n"
		"addq $120,%%rsp\n"
		"movw 8(%%rsp),%%ds\n"
		"movw (%%rsp),%%es\n"
		"addq $32, %%rsp\n"
		"iretq"
		: : "g"((uint64_t)tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile(
		/* Store registers that will be used. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* Fetch input once */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
		"movq %%r15, 0(%%rax)\n"
		"movq %%r14, 8(%%rax)\n"
		"movq %%r13, 16(%%rax)\n"
		"movq %%r12, 24(%%rax)\n"
		"movq %%r11, 32(%%rax)\n"
		"movq %%r10, 40(%%rax)\n"
		"movq %%r9, 48(%%rax)\n"
		"movq %%r8, 56(%%rax)\n"
		"movq %%rsi, 64(%%rax)\n"
		"movq %%rdi, 72(%%rax)\n"
		"movq %%rbp, 80(%%rax)\n"
		"movq %%rdx, 88(%%rax)\n"
		"pop %%rbx\n" // Saved rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // Saved rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // Saved rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // read the current rip.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		: : "g"(tf_cur), "g"(tf) : "memory");
}

/* do_schedule - 새 스레드를 스케줄합니다. 진입시 인터럽트가 꺼져 있어야 한다.
 * 이 함수는 현재 스레드의 상태를 status로 변경한 다음 다른 스레드를 찾아 실행한다.
 * schedule()에서 printf()를 호출하는 것은 안전하지 않다.
 * 매 스케줄 실행 시마다 destruction_req 리스트에 있는 스레드를 해제한다.
 */
static void do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);
	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current()->status = status;
	schedule();
}

/*
 * schedule() - 실행할 다음 스레드를 선택하고 현재 실행 중인 스레드를 다음 스레드로 교체한다.
 */
static void schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate(next);
#endif

	if (curr != next)
	{
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
			list_remove(&curr->a_elem);
		}

		// 스레드를 전환하기 전에 먼저 현재 실행 중인 스레드를 저장한다.
		thread_launch(next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}

/* higher_priority - ready_list와 waiter를 우선순위 내림차순으로 정렬하기 위한 비교 함수.
 */
bool higher_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	struct thread *ta = list_entry(a, struct thread, elem);
	struct thread *tb = list_entry(b, struct thread, elem);
	if (ta->priority > tb->priority) {
        return true;
    } else if (ta->priority < tb->priority) {
        return false;
    } else {
        // If priorities are equal, compare thread IDs (or creation time, etc.)
        return ta->tid < tb->tid;
    }
}

/* less_wakeup_ticks - sleep_list를 정렬하기 위한 비교 함수. list_less_func typedef로 선언되어 있다.
 */
bool less_wakeup_ticks(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	struct thread *ta = list_entry(a, struct thread, elem);
	struct thread *tb = list_entry(b, struct thread, elem);
	return ta->wakeup_ticks < tb->wakeup_ticks;
}

/* thread_sleep - 현재 실행 중인 스레드들 재운다.
 * sleep_list에 wakeup_ticks 기준으로 오름차순 삽입하고 스레드의 상태를 BLOCKED 상태로 전환한다.
 * thread_block() 내부적으로 schedule()을 호출하여 스케줄링을 수행한다.
 * 
 * Idle 스레드는 thread_sleep()을 호출할 수 없다.
 * 스레드 리스트를 조작할때, 인터럽트를 비활성화하고 조작이 끝나면 다시 활성화해야 한다.
 */
void thread_sleep(int64_t ticks)
{
	enum intr_level old_level = intr_disable();
	struct thread *t = thread_current();

	ASSERT(t != idle_thread);

	t->wakeup_ticks = ticks;
	list_insert_ordered(&sleep_list, &t->elem, (list_less_func *)less_wakeup_ticks, NULL);
	thread_block();
	intr_set_level(old_level);
}

/* thread_wakeup - 잠들어 있는 스레드를 깨운다.
 * sleep_list를 순회하면서 wakeup_ticks이 현재 os_ticks보다 작아진 스레드를 깨운다.
 * sleep_list는 wakeup_ticks 기준으로 오름차순으로 정렬되어 있다.
 * 깨어난 스레드는 READY 상태로 전환되고 ready_list에 priority 기준으로 내림차순 삽입된다.
 * 
 * 이 함수는 타이머 인터럽트 핸들러에서 호출된다. 따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행된다.
 * 스레드 리스트를 조작할때, 인터럽트를 비활성화하고 조작이 끝나면 다시 활성화해야 한다.
 */
void thread_wakeup(int64_t os_ticks)
{
	if (list_empty(&sleep_list))
		return;
	enum intr_level old_level = intr_disable();
	struct thread *t;

	while (!list_empty(&sleep_list))
	{
		t = list_entry(list_front(&sleep_list), struct thread, elem);
		if (t->wakeup_ticks > os_ticks)
			break;
		list_pop_front(&sleep_list);
		list_insert_ordered(&ready_list, &t->elem, (list_less_func *)higher_priority, NULL);
		t->status = THREAD_READY;
	}
	intr_set_level(old_level);
}