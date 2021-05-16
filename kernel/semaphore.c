/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * Distributed under the terms of the GNU GPL, version 2
 *
 * This file implements counting semaphores.
 * A counting semaphore may be acquired 'n' times before sleeping.
 * See mutex.c for single-acquisition sleeping locks which enforce
 * rules which allow code to be debugged more easily.
 */

/*
 * Some notes on the implementation:
 *
 * The spinlock controls access to the other members of the semaphore.
 * down_trylock() and up() can be called from interrupt context, so we
 * have to disable interrupts when taking the lock.  It turns out various
 * parts of the kernel expect to be able to use down() on a semaphore in
 * interrupt context when they know it will succeed, so we have to use
 * irqsave variants for down(), down_interruptible() and down_killable()
 * too.
 *
 * The ->count variable represents how many more tasks can acquire this
 * semaphore.  If it's zero, there may be tasks waiting on the wait_list.
 */

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/ftrace.h>

static noinline void __down(struct semaphore *sem);
static noinline int __down_interruptible(struct semaphore *sem);
static noinline int __down_killable(struct semaphore *sem);
static noinline int __down_timeout(struct semaphore *sem, long jiffies);
static noinline void __up(struct semaphore *sem);

/**
 * down - acquire the semaphore
 * @sem: the semaphore to be acquired
 *
 * Acquires the semaphore.  If no more tasks are allowed to acquire the
 * semaphore, calling this function will put the task to sleep until the
 * semaphore is released.
 *
 * Use of this function is deprecated, please use down_interruptible() or
 * down_killable() instead.
 */
void down(struct semaphore *sem)
{
	unsigned long flags;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		__down(sem);    // 注意   __down 会导致睡眠，所以在函数内部睡眠前会临时释放 spinlock
	spin_unlock_irqrestore(&sem->lock, flags);
}
EXPORT_SYMBOL(down);

/**
 * down_interruptible - acquire the semaphore unless interrupted
 * @sem: the semaphore to be acquired
 *
 * Attempts to acquire the semaphore.  If no more tasks are allowed to
 * acquire the semaphore, calling this function will put the task to sleep.
 * If the sleep is interrupted by a signal, this function will return -EINTR.
 * If the semaphore is successfully acquired, this function returns 0.
 */
int down_interruptible(struct semaphore *sem)
{
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;     // 获取 semaphore 成功
	else
		result = __down_interruptible(sem);    // 获取   semaphore 失败
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}
EXPORT_SYMBOL(down_interruptible);

/**
 * down_killable - acquire the semaphore unless killed
 * @sem: the semaphore to be acquired
 *
 * Attempts to acquire the semaphore.  If no more tasks are allowed to
 * acquire the semaphore, calling this function will put the task to sleep.
 * If the sleep is interrupted by a fatal signal, this function will return
 * -EINTR.  If the semaphore is successfully acquired, this function returns
 * 0.
 */
int down_killable(struct semaphore *sem)
{
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		result = __down_killable(sem);
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}
EXPORT_SYMBOL(down_killable);

/**
 * down_trylock - try to acquire the semaphore, without waiting
 * @sem: the semaphore to be acquired
 *
 * Try to acquire the semaphore atomically.  Returns 0 if the mutex has
 * been acquired successfully or 1 if it it cannot be acquired.
 *
 * NOTE: This return value is inverted from both spin_trylock and
 * mutex_trylock!  Be careful about this when converting code.
 *
 * Unlike mutex_trylock, this function can be used from interrupt context,
 * and the semaphore can be released by any task or interrupt.
 */
int down_trylock(struct semaphore *sem)
{
	unsigned long flags;
	int count;

	spin_lock_irqsave(&sem->lock, flags);
	count = sem->count - 1;
	if (likely(count >= 0))
		sem->count = count;
	spin_unlock_irqrestore(&sem->lock, flags);

	return (count < 0);
}
EXPORT_SYMBOL(down_trylock);

/**
 * down_timeout - acquire the semaphore within a specified time
 * @sem: the semaphore to be acquired
 * @jiffies: how long to wait before failing
 *
 * Attempts to acquire the semaphore.  If no more tasks are allowed to
 * acquire the semaphore, calling this function will put the task to sleep.
 * If the semaphore is not released within the specified number of jiffies,
 * this function returns -ETIME.  It returns 0 if the semaphore was acquired.
 */
int down_timeout(struct semaphore *sem, long jiffies)
{
	unsigned long flags;
	int result = 0;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		result = __down_timeout(sem, jiffies);
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}
EXPORT_SYMBOL(down_timeout);

/**
 * up - release the semaphore
 * @sem: the semaphore to release
 *
 * Release the semaphore.  Unlike mutexes, up() may be called from any
 * context and even by tasks which have never called down().
 */
void up(struct semaphore *sem)
{
	unsigned long flags;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(list_empty(&sem->wait_list)))
		sem->count++;    // 没有进程在等待本 semaphore， 则直接将 count +1
	else
		__up(sem);      // 否则需要唤醒正在等待本 semaphore 的进程
	spin_unlock_irqrestore(&sem->lock, flags);
}
EXPORT_SYMBOL(up);

/* Functions for the contended case */

struct semaphore_waiter {
	struct list_head list;
	struct task_struct *task;
	int up;
};

/*
 * Because this function is inlined, the 'state' parameter will be
 * constant, and thus optimised away by the compiler.  Likewise the
 * 'timeout' parameter for the cases without timeouts.
 */
static inline int __sched __down_common(struct semaphore *sem, long state,
								long timeout)
{
	struct task_struct *task = current;
	struct semaphore_waiter waiter;    // 用于描述每个获取 semaphore 失败的进程

	list_add_tail(&waiter.list, &sem->wait_list);   // add 到 semaphore 管理的睡眠进程链表中
	waiter.task = task;
	waiter.up = 0;

	for (;;) {
		if (signal_pending_state(state, task))
			goto interrupted;
		if (timeout <= 0)
			goto timed_out;
		__set_task_state(task, state);     // 设置当前进程状态（TASK_UNINTERRUPTIBLE，TASK_INTERRUPTIBLE）
		spin_unlock_irq(&sem->lock);       // 接下去要 schedule 了, 所以临时释放 spinlock
		timeout = schedule_timeout(timeout);     // 主动让出 cpu
		spin_lock_irq(&sem->lock);
		if (waiter.up)  // 如果被 up   操作唤醒则直接返回 0 表示成功获取了锁进入了临界区， 否则被其他操作(signal,timeout)唤醒则再度循环后通过 goto 跳出循环
			return 0;
	}

 timed_out:
	list_del(&waiter.list);
	return -ETIME;

 interrupted:
	list_del(&waiter.list);
	return -EINTR;
}

static noinline void __sched __down(struct semaphore *sem)
{
	__down_common(sem, TASK_UNINTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}

static noinline int __sched __down_interruptible(struct semaphore *sem)
{
	return __down_common(sem, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}

static noinline int __sched __down_killable(struct semaphore *sem)
{
	return __down_common(sem, TASK_KILLABLE, MAX_SCHEDULE_TIMEOUT);
}

static noinline int __sched __down_timeout(struct semaphore *sem, long jiffies)
{
	return __down_common(sem, TASK_UNINTERRUPTIBLE, jiffies);
}

static noinline void __sched __up(struct semaphore *sem)
{
	struct semaphore_waiter *waiter = list_first_entry(&sem->wait_list,
						struct semaphore_waiter, list);   // 睡眠时是 list_add_tail, 所以取队头，采用先进先出策略
	list_del(&waiter->list);
	waiter->up = 1;          // 标记被 up 唤醒了, 在 down 中判断该标记为 1 则直接返回 0 表示获取了 semaphore 进入了临界区
	wake_up_process(waiter->task);   // 唤醒进程
}
