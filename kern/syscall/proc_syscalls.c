#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
  // TO DO:
  // Need to perform process assignment even without/before any fork calls.
  //   The first user process might call getpid before creating any children. 
  //   sys_getpid needs to return a valid PID for this process.
  return kproc->pid;
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
#endif 
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
struct lock *pidCountLock = lock_create("pidCount");

int
sys_fork(pid_t pid, struct trapframe *tf) { 

    /* Create process structure for child process */
    struct proc *childProc = proc_create_runprogram("childProc");
    if (childProc == NULL) {
        return -1;
    }
    /* Create and copy address space */
    struct addrspace *oldas = curproc->p_addrspace;
    struct addrspace *newas;
    DEBUG(DB_LOCORE,"Fork110: oldas(%d) newas(%d)\n",sizeof(oldas),sizeof(newas));
    // as_copy: creates a new address spaces, and copies 
    // the pages from the old address space to the new one
    int as_copy_status = as_copy(oldas, &newas);
    // if as_copy return an error
    if (as_copy_status > 0){
        return as_copy_status;
    }
    DEBUG(DB_LOCORE,"Fork118: oldas(%d) newas(%d)\n",sizeof(oldas),sizeof(newas));
    // associate address space with the child process
    spinlock_acquire(&childProc->p_lock);
    childProc->p_addrspace = newas;
    spinlock_release(&childProc->p_lock);
    DEBUG(DB_LOCORE,"Fork123: childas(%d)\n",sizeof(childProc->p_addrspace));

    /* Assign PID to child process and create the parent/child relationship */
    lock_acquire(pidCountLock);
      pidCount += 1;
    lock_release(pidCountLock);
    childProc->pid = pidCount;
    // current process's pid = pid?
    childProc->parent = pid;

    /* Create thread for child process */
    // TO DO: trapframe synchronization
    thread_fork("childProc", childProc, 
                enter_forked_process(tf, pidCount), tf, pidCount);


    return (0);
}
#endif

