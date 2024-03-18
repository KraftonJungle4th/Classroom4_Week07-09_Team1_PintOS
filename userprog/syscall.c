#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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

/* 
* syscall_handler()는 인터럽트 핸들러로서, 인터럽트가 발생하면 호출된다.
* 인터럽트가 발생하면, 인터럽트 핸들러는 인터럽트 번호를 확인하고, 해당하는 핸들러를 호출한다.
 */
void syscall_handler (struct intr_frame *f) {

	int sys_name = f->R.rax;
	uint64_t syscall_num = *(uint64_t *) f->rsp;

	switch (sys_name)
	{
	case : SYS_HALT
		case SYS_HALT:
			halt();

		case SYS_EXIT:
			exit(f->R.rdi);

		case SYS_FORK:
			fork(f->R.rdi);		

		case SYS_EXEC:
			exec(f->R.rdi);

		case SYS_WAIT:
			wait(f->R.rdi);

		case SYS_CREATE:
			create(f->R.rdi, f->R.rsi);	

		case SYS_REMOVE:
			remove(f->R.rdi);		

		case SYS_OPEN:
			open(f->R.rdi);		

		case SYS_FILESIZE:
			filesize(f->R.rdi);

		case SYS_READ:
			read(f->R.rdi, f->R.rsi, f->R.rdx);

		case SYS_WRITE:
			write(f->R.rdi, f->R.rsi, f->R.rdx);	

		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);		

		case SYS_TELL:
			tell(f->R.rdi);		

		case SYS_CLOSE:
			close(f->R.rdi);	

	}

	thread_exit ();
}


/*
* check_address()는 주소가 유저 영역에 속하는지 확인한다.
* 만약 주소가 유저 영역에 속하지 않는다면, 프로세스를 종료한다.
* 	유저 영역 : 주소가 NULL이거나, 유저 영역에 속하지 않는 다른 주소들
*/
void check_address(void *addr) {
	struct thread *t = thread_current();

	if (!is_user_vaddr(adrr) || addr == NULL || pml4_get_page(t->pml4, addr) == NULL) {
		exit(-1);
	}
}

/*
* power_off()(src/include/threads/init.h에 선언)를 호출해 Pintos를 종료한다.
* 이 함수는 되도록 사용되지 않아야한다.
* -> deadlock 상황에 대한 정보 등을 약간 잃어 버릴 가능성 존재하기 때문
*/
void halt(void) {
	power_off();
}

/*
* 현재 동작중인 유저 프로그램을 종료한다.
* 커널에 상태를 리턴하면서 종료한다.
* 만약 부모 프로세스가 현재 유저 프로그램의 종료를 기다리던 중이라면, 종료되면서 리턴될 그 상태를 기다린다는 것이다.
* 상태 = 0이면 성공, 0이 아닌 값들은 에러를 뜻한다.
*/
void exit(int status) {
	struct thread *cur = thread_current();
	printf("%s: exit(%d)\n", cur->name, status);
	thread_exit();
}

/*
* 이름이 file(첫 번째 인자)이고 크기가 initial_size(두 번째 인자)인 새로운 파일을 생성한다. 
* 파일이 성공적으로 생성되었다면 true를 반환, 실패하면 false를 반환한다. 
* 새로운 파일을 생성하는 것이 파일을 여는 것은 아니다
*	—> 파일을 여는 것은 open system call의 역할로, ‘생성’과 개별적인 연산이다.
*/
void create(const char *file, unsigned initial_size) {
	check_address(file);
	bool success = filesys_create(file, initial_size);
	f->R.rax = success;
	return success;
}

/*
* 이름이 file(첫 번째 인자)인 파일을 삭제한다. 
* 성공적으로 삭제했다면 true를 반환, 실패하면 false를 반환한다.
* 파일은 열려있는지 닫혀있는지 여부와 관계없이 삭제될 수 있고,
*  파일을 삭제하는 것이 그 파일을 닫았다는 것을 의미하지 않는다. 
*/
void remove(const char *file) {
	check_address(file);
	bool success = filesys_remove(file);
	f->R.rax = success;
}

/*
* buffer로부터 open file fd로 size 바이트를 적는다.
* 실제로 적힌 바이트의 수를 반환하고, 일부 바이트가 적히지 못했다면 size보다 더 작은 바이트 수가 반환될 수 있다.
* fd가 1이면, 콘솔에 쓰기를 시도한다.
*	—> 콘솔에 쓰기를 시도하는 것은 putbuf() 함수
*/
int write(int fd, const void *buffer, unsigned length) {
	check_address(buffer);
	if (fd == 1) {
		putbuf(buffer, length);
	}
	return length;
}

int open(const char *file) {
	check_address(file);
	struct file *f = filesys_open(file);
	if (f == NULL) {
		return -1;
	}
	int fd = add_file_to_fd_table(f);
	if(fd == -1) {
		file_close(f);
	}

	return fd;
}

int add_file_to_fd_table(struct file *f) {
	struct thread *t = thread_current();
	struct file **fdt = t->file_descriptor_table;
	int fd = t->fd_index;

	while (t->file_descriptor_table[fd] != NULL && fd < FDCOUNT_LIMIT) {
		fd++;
	}
	if (fd >= FDCOUNT_LIMIT) {
		return -1;
	}
	t->fdidx = fd;
	fdt[fd] = file;
	return fd;
}

int filesize(int fd) {
	struct file *f = get_file_from_fd_table(fd);
	if (f == NULL) {
		return -1;
	}
	return file_length(f);
}