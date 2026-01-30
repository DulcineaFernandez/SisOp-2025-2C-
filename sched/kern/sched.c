#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

static uint32_t sched_ticks;
#ifdef SCHED_PRIORITIES
void
sched_boost(void)
{
	const uint32_t INTERVALO = 200;
	sched_ticks += 1;
	if (sched_ticks % INTERVALO == 0) {
		for (int i = 0; i < NENV; i++) {
			if (envs[i].env_status != ENV_FREE) {
				envs[i].vruntime = 0;
			}
		}
	}
}
#endif

// Choose a user environment to run and run it.
void
sched_yield(void)
{
#ifdef SCHED_ROUND_ROBIN
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running. Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	int start;
	int i;

	if (curenv) {
		start = ENVX(curenv->env_id) + 1;
	} else {
		start = 0;
	}

	for (i = 0; i < NENV; i++) {
		int idx = (start + i) % NENV;

		if (envs[idx].env_status == ENV_RUNNABLE) {
			env_run(&envs[idx]);
		}
	}

	if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	}
#endif

#ifdef SCHED_PRIORITIES
	// Implement simple priorities scheduling.
	//
	// Environments now have a "priority" so it must be consider
	// when the selection is performed.
	//
	// Be careful to not fall in "starvation" such that only one
	// environment is selected and run every time.
	struct Env *mejor_candidato = NULL;
	uint32_t mejor_vr = ~0u;  // ~0u maximo numero posible
	static bool flip = false;
	for (int i = 0; i < NENV; i++) {
		if (envs[i].env_status == ENV_RUNNABLE) {
			if ((envs[i].vruntime < mejor_vr) || (envs[i].vruntime == mejor_vr && flip)) {
				mejor_vr = envs[i].vruntime;
				mejor_candidato = &envs[i];
			}
		}
	}
	flip = !flip;
	if (mejor_candidato) {
		env_run(mejor_candidato);
	} else if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	}

#endif
	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		for (int j = 0; j < NENV; j++) {
			if (envs[j].env_id) {
				cprintf("Proceso (env) %08x runs: %d runtime: "
				        "%d vruntime: %d prio: %d \n",
				        envs[j].env_id,
				        envs[j].env_runs,
				        envs[j].runtime,
				        envs[j].vruntime,
				        envs[j].priority);
			}
		}
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
