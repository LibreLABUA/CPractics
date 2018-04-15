#include <pty.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define nil NULL

int conn;
struct termios term, ancterm;

int dial(char *, char *);
void handle(int);

void
restore()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &ancterm);
}

void
sighandler(int s)
{
  restore();
  close(conn);
}

int
main()
{
  conn = dial("mester.pw", "7777");
  if (conn == -1)
    {
      perror("dial()");
      exit(1);
    }

  signal(SIGINT, sighandler);

  printf("connected\n");
  handle(conn);
  close(conn);
  restore();
}
  
int
dial(char *host, char *port)
{
  int n, conn;
  struct addrinfo addr = {
    0, AF_INET, SOCK_STREAM, 0,
    0, NULL, NULL, NULL
  };
  struct addrinfo *res, *p;

  n = getaddrinfo(host, port, &addr, &res);
  if (n != 0)
    return -1;

  for (p = res; p != nil; p = p->ai_next)
    {
      conn = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (conn != -1)
        if (connect(conn, p->ai_addr, p->ai_addrlen) != -1)
          break;
      conn = -1;
    }

  freeaddrinfo(res);
  return conn;
}

void
handle(int conn)
{
  int n, i;
  char buf;
  fd_set fds, readfds;

  tcgetattr(STDIN_FILENO, &term);
  ancterm = term;
  cfmakeraw(&term);
  tcsetattr(STDIN_FILENO, TCSANOW, &term);

  FD_ZERO(&fds);
  FD_SET(conn, &fds);
  FD_SET(STDIN_FILENO, &fds);

  readfds = fds;

  int fdi, fdo;
  for (;;)
    {
      fds = readfds;
      n = select(FD_SETSIZE, &fds, nil, nil, nil);
      if (n == -1)
        break;
      if (n == 0) continue;

      if (FD_ISSET(conn, &fds))
        fdi = conn, fdo = STDOUT_FILENO;
      else
        fdi = STDIN_FILENO, fdo = conn;

      n = read(fdi, &buf, 1);
      if (n < 1)
        break;
      n = write(fdo, &buf, 1);
      if (n < 1)
        break;
    }
}
