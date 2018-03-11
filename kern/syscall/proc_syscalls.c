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
#include <limits.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  
#if OPT_A2

  // save exitcode for parent to retrieve
  p->exitStatus = _MKWAIT_EXIT(exitcode);

  // For child, if parents live, wake them up
  if (p->parent != -1){
    cv_broadcast(p->waitExit, p->waitExitLock);
  } 

  // For parents, find children,
  unsigned int n = array_num(processArray);
  for (unsigned int i = 0; i < n; i++){
    struct proc *pidProc = array_get(processArray, i);
    if (pidProc == NULL) {
        continue;
    }
    if (p->pid == pidProc->parent){
      
      // if any live children, detach the children and parent relationship
      if (pidProc->exitStatus != -1){
        pidProc->parent = -1;
      }
	    // release children's exit lock, so that they can exit
      lock_release(pidProc->exitLock); 
    }
  }
  // For child, if my parent is died, fully delete myself, b/c no parent can call waitpid
  // For parent, fully delete myself
  // Meaning, if no living parent, fully delete themself
  // proc_destroy(p);
  
  
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

#if OPT_A2
  // prevent child process exit before parent process
  lock_acquire(p->exitLock);
  lock_release(p->exitLock);
#endif

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
  *retval = curproc->pid;
  // TO DO:
  // Need to perform process assignment even without/before any fork calls.
  //   The first user process might call getpid before creating any children. 
  //   sys_getpid needs to return a valid PID for this process.
  // (Done!)
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
#endif
  return (0); 
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
  
  if (options != 0) {
    return(EINVAL);
  }
#if OPT_A2
  struct proc *p = curproc;
  struct proc *pidProc = array_get(processArray, pid);
  // pid is not your child
  if (pidProc->parent != p->pid){
    return ECHILD;
  }
  // The pid argument named a nonexistent process.
  if (pidProc == NULL){
    return ESRCH;
  }
  // The status argument was an invalid pointer.
  if(status == NULL){
    return EFAULT;
  }
  
  
  // while the child hasn't exited, go to sleep.
  // cv on the child process
  lock_acquire(pidProc->waitExitLock);
  while (pidProc->exitStatus == -1){
    cv_wait(pidProc->waitExit, pidProc->waitExitLock); 
  }
  // get the exitstatus from child
  exitstatus = pidProc->exitStatus;
  lock_release(pidProc->waitExitLock);

  // Parent get the child's exit code, allow the child to exit
  //lock_release(pidProc->exitLock);

#else
  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif 

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
  //There are already too many processes on the system.
  if (array_num(processArray) >= __PID_MAX) {
    // EMPROC - The current user already has too many processes.
    // Since there is only one user, EMPROC and ENPROC are the same.
    return ENPROC;
  }

  /* Create process structure for child process */
  struct proc *childProc = proc_create_runprogram("childProc");
  if (childProc == NULL) {
      return ENOMEM;
  }
  lock_acquire(childProc->exitLock);
  
  /* Create and copy address space */
  struct addrspace *oldas = curproc->p_addrspace;
  struct addrspace *newas;
  // as_copy: creates a new address spaces, and copies 
  // the pages from the old address space to the new one
  int as_copy_status = as_copy(oldas, &newas);
  // if as_copy return an error
  if (as_copy_status != 0){
      kfree(oldas);
      kfree(newas);
      proc_destroy(childProc);
      return as_copy_status;
  }

  // associate address space with the child process
  spinlock_acquire(&childProc->p_lock);
  childProc->p_addrspace = newas;
  spinlock_release(&childProc->p_lock);

  //DEBUG(DB_LOCORE,"Fork118: oldas(%d) newas(%d)\n",oldas->as_vbase1,newas->as_vbase1);
  DEBUG(DB_LOCORE,"Fork202: childProc->pid = %d \n", childProc->pid);
  DEBUG(DB_LOCORE,"Fork202: curProc->pid = %d \n", curproc->pid);
  /* Create the parent/child relationship */
  // current process's pid
  childProc->parent = curproc->pid;

  /* Create thread for child process */

  // Can we just pass the parent's trapframe pointer to thread_fork?
  // No, parent process may exit before child thread created, parent's trapframe may already be deleted.
  // Solution: Copy parents trapframe on the heap 
  // and pass a pointer to the copy into the entrypoint function.
  struct trapframe *childTf = kmalloc(sizeof(struct trapframe));
  if(childTf == NULL){
    proc_destroy(childProc);
    return ENOMEM;
  }
  *childTf = *tf;
  // create a new thread
  int forkResult = thread_fork("childProc", childProc, enter_forked_process, childTf, 0);
  if(forkResult != 0){
    kfree(oldas);
    kfree(newas);
    kfree(childTf);
    proc_destroy(childProc);
    return forkResult;
  }

  *retval = childProc->pid;
  return 0;
}
#endif


#if OPT_A2

int
sys_execv(const_userptr_t progname, userptr_t args) { 
  /* Count the number of arguments and copy them into the kernel */
  //int argc = strlen(args[0]); 
  (void) args;
  /* Copy the program path into the kernel */
  size_t progLength = strlen((char *)progname) + 1;
  char *progPath = kmalloc(sizeof(char)*progLength);
  copyinstr(progname, progPath, progLength, &progLength); 

  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;

  /* Open the file. */
  result = vfs_open(progPath, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }

  /* We should be a new process. */
  // KASSERT(curproc_getas() == NULL);

  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Need to copy the arguments into the new address space. 
  Consider copying the arguments (both the array and the strings) 
  onto the user stack as part of as_define_stack. */
  



  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return result;
  }

  //kfree(progPath);
  
  /* Delete old address space */
  as_destroy(as);
  
  /* Warp to user mode. */
  enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
        stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;

}
#endif

