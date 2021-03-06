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

pid_t pid;
struct termios tio;

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
  if (read_bytes > 0) {
    written_bytes = write(to, buf, read_bytes);
    fsync(to);
  }
  return read_bytes;
}

int
serve_process(int fd)
{
  fd_set fds;
  int res;

  for (;;) {
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_SET(fd, &fds);

    res = select(fd + 1, &fds, 0, 0, 0);
    if (res > 0) {
      if (FD_ISSET(0, &fds)) {
        if (pump_data(0, fd) <= 0)
          break;
      }
      if (FD_ISSET(fd, &fds)) {
        if (pump_data(fd, 1) < 0)
          break;
      }
    } else {
      if (errno != EINTR)
        break;
    }
  }
  return 0;
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
  int i = 1, fd, noecho = 0, ret = 0;

  signal(SIGINT, handle_sigint);

  for (i = 1; i < argc && argv[i][0] == '-'; ++i) {
    switch (argv[i][1]) {
      case 'n': noecho = 1; break;
      default: ret = 1; break;
    }
  }

  if (tcgetattr(0, &tio) < 0)
    return 1;
  if (noecho)
    set_noecho(0);

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
      serve_process(fd);
      close(fd);
      wait(0);
      tcsetattr(0, TCSANOW, &tio);
      return 0;
    }
  } else {
    fprintf(stderr, "Usage: %s [-n] cmd [arg1] ...\n", argv[0]);
  }
}
