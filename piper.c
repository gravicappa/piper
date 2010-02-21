#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>
#include <errno.h>

#define FIFO_NAME ":in"

void
log_printf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fflush(stderr);
}

int
pump_data(int from, int to)
{
  char buf[1024];
  size_t read_bytes, written_bytes = 0;

  read_bytes = read(from, buf, sizeof(buf));
#if 0
  if (read_bytes == -1)
    perror("pump_data");
#endif
  if (read_bytes > 0) {
    written_bytes = write(to, buf, read_bytes);
    fsync(to);
  }
  return read_bytes;
}

int
serve_process(int fd, int fifo)
{
  fd_set fds;
  int res, m;

  m = (fd > fifo) ? fd : fifo;
  for (;;) {
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_SET(fd, &fds);
    FD_SET(fifo, &fds);

    res = select(m + 1, &fds, 0, 0, 0);
    if (res > 0) {
      if (FD_ISSET(0, &fds)) {
        if (pump_data(0, fd) <= 0)
          break;
      }
      if (FD_ISSET(fd, &fds)) {
        if (pump_data(fd, 1) < 0)
          break;
      }
      if (FD_ISSET(fifo, &fds)) {
        pump_data(fifo, fd);
      }
    } else {
      break;
    }
  }
  return 0;
}

int
start_fifo(const char *name, int *rfifo, int *wfifo)
{
  remove(name);
  if (mkfifo(name, 0600))
    return 1;

  *rfifo = open(name, O_RDONLY | O_NONBLOCK);
  if (*rfifo < 0)
    goto error;

  *wfifo = open(name, O_WRONLY);
  if (*wfifo < 0)
    goto error;

  return 0;

error:
  if (*rfifo)
    close(*rfifo);
  if (*wfifo)
    close(*wfifo);
  remove(name);
  return 1;
}

int
main(int argc, char **argv)
{
  char *fifo_name = FIFO_NAME;
  int i, fd, wfifo, fifo, ret = 0;
  pid_t pid;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-f") == 0) {
      if (i + 1 < argc) {
        fifo_name = argv[++i];
      } else {
        ret = 1;
      }
    } else
      break;
  }
  if (i < argc && ret == 0) {
    ret = 0;
    switch (forkpty(&fd, 0, 0, 0)) {
      case -1:
        perror("forkpty");
        return -1;

      case 0:
        execvp(argv[i], argv + i);
        perror("Running process error");
        return -1;

      default:
        if (start_fifo(fifo_name, &fifo, &wfifo) == 0) {
          serve_process(fd, fifo);
          ret = 0;
          close(fd);
          close(fifo);
          close(wfifo);
          remove(fifo_name);
          wait(0);
        }
    }
  } else {
    fprintf(stderr, "Usage: %s [-f fifo_name] cmd ...\n", argv[0]);
  }
  return ret;
}
