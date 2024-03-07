/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* sema_down - 세마포어에 대한 Down 또는 P 연산이다.
 * 세마포어 값이 양수가 될 때까지 기다렸다가 원자적으로 감소시킨다.
 * 이 함수는 BLOCKED 될 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안된다.
 * 이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, 
 * BLOCKED가 발생하면 다음 스케줄링된 스레드가 인터럽트를 다시 활성화 할 수 있다.
 */
void sema_down(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	ASSERT(!intr_context());
	old_level = intr_disable();
	while (sema->value == 0)
	{
		list_insert_ordered(&sema->waiters, &thread_current()->elem, (list_less_func *)&higher_priority, NULL);
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* sema_up - Up 또는 세마포어에서 V 연산을 수행한다.
 * 세마포어의 값을 증가시키고, 세마포어를 기다리는 스레드 중 하나를 깨운다 (있는 경우).
 * 현재 실행 중인 스레드가 양보하고, 스케줄링된다. 스케줄러 재량에 따라 다시 같은 스레드가 실행될 수 있다.
 * 이 함수는 인터럽트 핸들러에서 호출될 수 있다.
 */
void sema_up(struct semaphore *sema)
{
	struct thread *t;
	enum intr_level old_level;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (!list_empty(&sema->waiters)) {
		list_sort(&sema->waiters, (list_less_func *)&higher_priority, NULL);
		t = list_entry(list_pop_front(&sema->waiters), struct thread, elem);
		thread_unblock(t);
	}
	sema->value++;
	intr_set_level(old_level);
	thread_yield();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* lock_acquire - 잠금을 획득하고 필요한 경우 잠금을 사용할 수 있을 때까지 대기한다.
 * 잠금은 현재 스레드가 이미 보유하고 있지 않아야 한다.
 *
 * 잠금을 획득하지 못할 경우, 해당 잠금을 wait_on_lock에 저장한다.
 * 이후, 현재 스레드의 우선순위가 잠금을 보유하고 있는 스레드의 우선순위보다 높을 경우,
 * 잠금을 보유하고 있는 스레드의 donations 리스트에 현재 스레드를 추가한다.
 * 그리고 중첩 기부를 수행한다.
 * 
 * 이 함수는 BLOCKED 될 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안된다.
 * 이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, Sleep이 필요하면 인터럽트가 다시 활성화된다.
 */
void lock_acquire (struct lock *lock) {
	struct thread *t = thread_current();
	struct thread *holder;
	
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(!lock_held_by_current_thread(lock));

	if (lock->holder != NULL) {
		t->wait_on_lock = lock;
		if (thread_get_priority() > lock->holder->priority) {
			list_insert_ordered(&lock->holder->donations, &t->d_elem, (list_less_func *)&higher_priority, NULL);
			while (t->wait_on_lock != NULL) {
				holder = t->wait_on_lock->holder;
				if (t->priority > holder->priority) {
					holder->priority = t->priority;
					t = holder;
				}
				else break;
			}
		}
	}
	sema_down(&lock->semaphore);
	thread_current()->wait_on_lock = NULL;
	lock->holder = thread_current();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* lock_release - 현재 스레드가 소유하고 있는 잠금을 해제한다.
 *
 * 잠금이 해제되면, 해당 잠금을 가지고 있던 스레드의 donations 리스트에서 해당 잠금을 기다리고 있던 스레드를 삭제하고, 
 * 잠금을 기다리고 있던 스레드의 wait_on_lock을 NULL로 설정한다.
 * 그리고 다중 기부를 수행하기 위해 donations 리스트를 검사하여 현재 스레드의 우선순위를 재설정한다.
 * 만약 donations 리스트가 비어있지 않다면, 현재 스레드의 우선순위를 donations 리스트의 가장 높은 우선순위로 설정하고,
 * donations 리스트가 비어있다면, 현재 스레드의 우선순위를 원래의 우선순위로 설정한다.
 * 
 * 인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서 잠금을 해제하려고 시도하는 것은 의미가 없다.
 */
void lock_release (struct lock *lock) {
	ASSERT(lock != NULL);
	ASSERT(lock_held_by_current_thread(lock));

	struct thread *cur = lock->holder;
	struct thread *start = list_entry(list_begin(&cur->donations), struct thread, d_elem);
	struct thread *end = list_entry(list_end(&cur->donations), struct thread, d_elem);
	for (struct thread *idx = start; idx != end; idx = list_entry(list_next(&idx->d_elem), struct thread, d_elem)) {
		if (idx->wait_on_lock == lock) {
			list_remove(&idx->d_elem);
			idx->wait_on_lock = NULL;
		}
	}

	if (!list_empty(&cur->donations)) {
		cur->priority = list_entry(list_front(&cur->donations), struct thread, d_elem)->priority;
	}
	else
		cur->priority = cur->original_priority;

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* cond_wait - 잠금을 원자적으로 해제하고 다른 코드가 COND 신호를 보낼 때까지 기다린다.
 * COND가 신호를 받은 후 잠금을 다시 획득한 후 반환합니다.
 * 이 함수를 호출하기 전에 잠금을 유지해야 한다.
 * 
 * 이 함수가 구현하는 Monitor는 "Hoare" 스타일이 아닌 "Mesa" 스타일이다.
 * ( Mesa: 시그널을 보내거나 받는 것이 원자적인 작업이 아니다, Hoare: 시그널을 보내거나 받는 것이 원자적인 작업이다. )
 * 즉, Signal 송수신은 원자적인 작업이 아니다. 따라서 일반적으로 호출자는 대기가 완료된 후 조건을 다시 확인하고 필요한 경우 다시 대기해야 한다.
 * 
 * 주어진 조건 변수는 하나의 잠금에만 연결되지만, 하나의 잠금은 여러 개의 조건 변수와 연결될 수 있다.
 * 즉, 잠금에서 조건 변수로의 1대N 매핑이 존재한다.
 * 
 * 이 함수는 Sleep을 할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안된다.
 * 이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, Sleep이 필요하면 인터럽트가 다시 활성화된다.
 */
void cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_insert_ordered(&cond->waiters, &waiter.elem, (list_less_func *)&cond_priority, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* cond_signal - COND에 대기 중인 스레드 중 하나에게 신호를 보낸다.
 * 신호를 받은 스레드는 대기를 해제하고, 신호를 보낸 스레드가 잠금을 유지하고 있는 경우 잠금을 다시 획득한다.
 * 
 * 이 함수는 LOCK이 유지되어야 한다.
 * 
 * 인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서 조건 변수를 신호로 보내는 것은 의미가 없다.
 */
void cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		list_sort(&cond->waiters, (list_less_func *)&cond_priority, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

/* cond_priority - 조건 변수의 대기자 리스트에서 우선순위가 높은 스레드를 찾는다.
 */
bool cond_priority(const struct list_elem *a, const struct list_elem  *b, void *aux UNUSED) {
    struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
    struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);
    struct thread *t_a = list_entry(list_begin(&sema_a->semaphore.waiters), struct thread, elem);
    struct thread *t_b = list_entry(list_begin(&sema_b->semaphore.waiters), struct thread, elem);
    return t_a->priority > t_b->priority;
}