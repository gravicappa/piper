#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

pid_t pid;

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
serve_process(int fd, int exit_after_stdin_closed)
{
  fd_set fds;
  int res, m, stdin_closed = 0;

  for (;;) {
    FD_ZERO(&fds);
    if (!stdin_closed)
      FD_SET(0, &fds);
    FD_SET(fd, &fds);

    res = select(fd + 1, &fds, 0, 0, 0);
    if (res > 0) {
      if (FD_ISSET(0, &fds)) {
        if (pump_data(0, fd) <= 0) {
          stdin_closed = 1;
          if (exit_after_stdin_closed)
            break;
        }
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
set_raw(int fd)
{
  struct termios tis;

  if (tcgetattr(fd, &tis) < 0)
    return 1;

  tis.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL
                   | IXON);
  tis.c_oflag &= ~OPOST;
  tis.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tis.c_cflag &= ~(CSIZE | PARENB);
  tis.c_cflag |= CS8;

  if (tcsetattr(fd, TCSANOW, &tis) < 0)
    return 1;
  return 0;
}

void
handle_sigint(int sig)
{
  signal(SIGINT, handle_sigint);
  kill(pid, SIGINT);
}

void
usage(const char *argv0) 
{
  fprintf(stderr, "Usage: %s [-close-after] cmd [arg1] ...\n", argv0);
}

int
main(int argc, char **argv)
{
  int i, fd, ret = 0, status = 0, close_after = 0;

  for (i = 1; i < argc; ++i)
    if (strcmp(argv[i], "--") == 0)
      break;
    else if (strcmp(argv[i], "-h") == 0)
      usage(argv[0]);
    else if (strcmp(argv[i], "-close-after") == 0)
      close_after = 1;
    else
      break;
  signal(SIGINT, handle_sigint);

  if (i < argc && ret == 0) {
    ret = 0;
    switch ((pid = forkpty(&fd, 0, 0, 0))) {
      case -1:
        perror("forkpty");
        return -1;

      case 0:
        set_raw(0);
        execvp(argv[i], argv + i);
        perror("exec");
        return -1;

      default:
        serve_process(fd, !close_after);
        if (!close_after)
          close(fd);
        waitpid(pid, &status, 0);
        close(fd);
        if (WIFEXITED(status))
          ret = WEXITSTATUS(status);
        else
          ret = 1;
    }
  } else
    usage(argv[0]);
  return ret;
}
