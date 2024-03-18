#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void check_address(void *addr);
void halt(void);
void exit(int status);
void create(const char *file, unsigned initial_size);
void remove(const char *file);
int write(int fd, const void *buffer, unsigned size);
int open(const char *file);


#endif /* userprog/syscall.h */
