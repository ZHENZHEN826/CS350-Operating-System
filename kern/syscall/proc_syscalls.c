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
#include <synch.h>
#include <mips/trapframe.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  
#if OPT_A2
  /*
  // return exitcode?
  // For child, if parents live
  if (curthread->parent > 0 ){
    // save exitcode somewhere, for parent to retrieve
    // Exit code is passed to the parent process?
    cv_signal(my parent);
  }
  // For child, if my parents is died, fully delete myself, b/c no parent can call waitpid
  proc_destroy(curthread?);

  // For parents, check children, if any zombie children, delete them
  // if any live children, detach the children and parent relationship
  proc_destroy(child process);
  */
#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
#endif

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
  *retval = 1;
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

//#if OPT_A2
  /*
  // pid is not your child
  if (){
    return ECHILD
  }
  // The pid argument named a nonexistent process.
  if (){
    return ESRCH;
  }
  // The status argument was an invalid pointer.
  if(status == NULL){
    return EFAULT;
  }
  if (options != 0) {
    return(EINVAL);
  }

  // while the child hasn't exited, go to sleep, sync to make sure no context switch
  // cv to the child process
  while (){
   
  }
  // get the exitstatus somewhere
  
  exitstatus = __WEXITED;
  // what is exitcode?
  int exitcode = _MKWAIT_EXIT(exitstatus);
  // fire this child
  proc_destroy(THIS CHILD)
  */
//#else
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
//#endif 

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2

int
sys_fork(pid_t *retval, struct trapframe *tf) { 
    //The current user already has too many processes.
    /*
    if (){
      return EMPROC;
    }
    //There are already too many processes on the system.
    if () {
      return ENPROC;
    }
    */

    /* Create process structure for child process */
    struct proc *childProc = proc_create_runprogram("childProc");
    if (childProc == NULL) {
        return ENOMEM;
    }
    /* Create and copy address space */
    struct addrspace *oldas = curproc->p_addrspace;
    struct addrspace *newas;
    // as_copy: creates a new address spaces, and copies 
    // the pages from the old address space to the new one
    int as_copy_status = as_copy(oldas, &newas);
    // if as_copy return an error
    if (as_copy_status > 0){
        return as_copy_status;
    }
    childProc->p_addrspace = newas;
    DEBUG(DB_LOCORE,"Fork118: oldas(%d) newas(%d)\n",oldas->as_vbase1,newas->as_vbase1);
    // associate address space with the child process
    spinlock_acquire(&childProc->p_lock);
    childProc->p_addrspace = newas;
    spinlock_release(&childProc->p_lock);

    /* Assign PID to child process and create the parent/child relationship */
    lock_acquire(pidCountLock);
      pidCount += 1;
      childProc->pid = pidCount;
    lock_release(pidCountLock);
    DEBUG(DB_LOCORE,"Fork202: pidCount = %lu \n", pidCount);

    // current process's pid
    childProc->parent = curproc->pid;

    /* Create thread for child process */

    // Can we just pass the parent's trapframe pointer to thread_fork?
    // No, parent process may exit before child thread created, parent's trapframe may already be deleted.
    // Solution: Copy parents trapframe on the heap 
    // and pass a pointer to the copy into the entrypoint function.
    struct trapframe *childTf = kmalloc(sizeof(struct trapframe));
    *childTf = *tf;
    // create a new thread
    int forkResult = thread_fork("childProc", childProc, enter_forked_process, childTf, 0);
    if(forkResult != 0){
      proc_destroy(childProc);
      kfree(childTf);
      return forkResult;
    }

    *retval = pidCount;
    return 0;
}
#endif

