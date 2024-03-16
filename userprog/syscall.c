#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "include/threads/init.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int status);
pid_t fork (const char *thread_name);
int exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface
 * 1. 포인터가 유효하지 않은 경우
 * -> 현재 스택의 상단의 주소가 유저의 가상 주소가 아니면 종료
 * 2. 포인터가 커널 영역에 있는 경우
 * -> 
 * 3. 포인터가 가리키는 블록이 커널 영역에 부분적으로 있는 경우
 * 
 * 커널에서 시스템 호출이 발생 했을 때, 실행된다.
 * 시스템 호출을 처리하기 전에 유효한 주소인지 확인한 후, 시스템 호출이 안전하게 실행될 수 있도록 한다.
 */
void syscall_handler (struct intr_frame *f) {
	
	if (!is_user_vaddr(f->rsp)) {
		printf("rsp is not in user vaddr\n");
		thread_exit();
	}

	if (KERN_BASE < f->rsp || f->rsp < 0) {
		printf("rsp is in kernel vaddr\n");
		thread_exit();
	}

	if (KERN_BASE < f->rsp + 8 || f->rsp + 8 < 0) {
		printf("rsp partially is in kernel vaddr\n");
		thread_exit();
	}

	uint64_t syscall_num = f->R.rax;
	if (syscall_num == SYS_HALT) {
		halt();
	}

	printf ("system call!\n");
	thread_exit ();
}

void halt (void) {
	power_off();
}
