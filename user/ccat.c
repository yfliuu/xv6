#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"

char buf[512];

int
cc_read(int fd, char *buf, int size)
{
  return ccall(1, SYS_read, fd, (uint64)buf, size, 0);
}

int
cc_write(int fd, char *buf, int size)
{
  return ccall(1, SYS_write, fd, (uint64)buf, size, 0);
}

int cc_open(char *path, int omode)
{
  return ccall(1, SYS_open, (uint64)path, omode, 0, 0);
}

int cc_close(int fd)
{
  return ccall(1, SYS_close, fd, 0, 0, 0);
}

void
cat(int fd)
{
  int n;

  while((n = cc_read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, n);
  if(n < 0){
    printf(1, "cat: read error\n");
    exit();
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    cat(0);
    exit();
  }

  for(i = 1; i < argc; i++){
    if((fd = cc_open(argv[i], 0)) < 0){
      printf(1, "cat: cannot open %s\n", argv[i]);
      exit();
    }
    cat(fd);
    cc_close(fd);
  }
  exit();
}
