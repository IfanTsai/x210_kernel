#ifndef _ASMARM_CURRENT_H
#define _ASMARM_CURRENT_H

#include <linux/thread_info.h>

static inline struct task_struct *get_current(void) __attribute_const__;

/* 获取当前进程 */
static inline struct task_struct *get_current(void)
{
	/* 通过当前线程信息获取task_struct实例 (当前进程PCB) */
	return current_thread_info()->task;  
}

#define current (get_current())

#endif /* _ASMARM_CURRENT_H */
