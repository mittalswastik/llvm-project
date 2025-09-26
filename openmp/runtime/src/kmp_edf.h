#include <linux/types.h>
#include <linux/sched.h>

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

#if defined (__linux__) && !defined(HAVE_SCHED_SETATTR)
# include <sys/syscall.h>
#endif

#if defined (__linux__) && !defined(HAVE_SCHED_SETATTR) && defined(SYS_sched_setattr)
# define HAVE_SCHED_SETATTR

struct sched_attr {
	__u32 size;

	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;

	/* SCHED_DEADLINE (nsec) */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
 };

 int sched_setattr(pid_t pid,
		  const struct sched_attr *attr,
		  unsigned int flags)
 {
   //	return syscall(__NR_sched_setattr, pid, attr, flags);
	return syscall(SYS_sched_setattr, pid, attr, flags);
 }

 int sched_getattr(pid_t pid,
		  struct sched_attr *attr,
		  unsigned int size,
		  unsigned int flags)
 {
   //	return syscall(__NR_sched_getattr, pid, attr, size, flags);
	return syscall(SYS_sched_getattr, pid, attr, size, flags);
 }

 int getcpu(unsigned *cpu, unsigned *node)
 {
   //	return syscall(__NR_getcpy, cpu, node);
	return syscall(SYS_getcpu, cpu, node);
 }
#endif
