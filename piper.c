#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define FIFO_NAME ":in"

pid_t pid;

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
      if (errno != EINTR)
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

static int
set_noecho(int fd)
{
  struct termios stermios;

  if (tcgetattr(fd, &stermios) < 0)
    return 1;

  stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
  stermios.c_lflag |= ICANON;
  /* stermios.c_oflag &= ~(ONLCR); */
  /* would also turn off NL to CR/NL mapping on output */
  stermios.c_cc[VERASE] = 0177;
#ifdef VERASE2
  stermios.c_cc[VERASE2] = 0177;
#endif
  if (tcsetattr(fd, TCSANOW, &stermios) < 0)
    return 1;
  return 0;
}

void
handle_sigint(int sig)
{
  signal(SIGINT, handle_sigint);
  kill(pid, SIGINT);
}

int
main(int argc, char **argv)
{
  const char *fifo_name = FIFO_NAME;
  int i, fd, wfifo, fifo, ret = 0;

  for (i = 1; i < argc && argv[i][0] == '-'; ++i)
    switch (argv[i][1]) {
      case 'f':
        if (++i < argc)
          fifo_name = argv[i];
      break;
      default: ret = 1;
    }
  signal(SIGINT, handle_sigint);

  if (i < argc && ret == 0) {
    ret = 0;
    switch ((pid = forkpty(&fd, 0, 0, 0))) {
    case -1:
      perror("forkpty");
      return -1;

    case 0:
      set_noecho(0);
      execvp(argv[i], argv + i);
      perror("exec");
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
    fprintf(stderr, "Usage: %s [-f fifo_name] cmd [arg1] ...\n", argv[0]);
  }
  return ret;
}
