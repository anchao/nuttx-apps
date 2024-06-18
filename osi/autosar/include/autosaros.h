#ifndef AUTOSAROS_H
#define AUTOSAROS_H

#define TASK(Name, Section_Name)						\
	extern void OS_TASK_ENTRY_##Name(void) __attribute__((section("."#Section_Name".code")));	\
	void OS_TASK_ENTRY_##Name(void)

#define DECLARE_TASK(Name, Autostart, Prio, Flags, AppMode, CoreID,	\
	TaskTimeFrame, IsrTimeFrame, IsrBudget, TaskBuddget, AppId, SchedPolicy)	\
	extern void OS_TASK_ENTRY_##Name(void);		\
	struct osek_task_attr OS_TASK_ATTR_##Name =		\
	{								\
		.entry =OS_TASK_ENTRY_##Name,			\
		.prio = Prio,						\
		.auto_start = Autostart,				\
		.flags = Flags,						\
		.app_mode = AppMode,					\
		.core_id = CoreID,					\
		.cp_task_time_frame = TaskTimeFrame,			\
		.cp_isr_time_frame = IsrTimeFrame,			\
		.cp_task_execution_budget = TaskBuddget,		\
		.cp_isr_execution_budget = IsrBudget,			\
		.app_id = AppId,						\
		.sched_policy = SchedPolicy \
	}

#endif
