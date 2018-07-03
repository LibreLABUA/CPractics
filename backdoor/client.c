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
void panic(const char *);

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

void
help(const char *arg0)
{
  fprintf(stderr, "%s [options]\n"
      "  -p: Remote port\n"
      "  -h: Remote host\n", arg0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int n;
  char *host, *port;

  host = port = nil;
  /* Getting simple arguments */
  while ((n = getopt(argc, argv, "p:h:")) != -1){
    switch (n){
      case 'h':
        host = optarg;
        break;
      case 'p':
        port = optarg;
        break;
    }
  }
  if(host == nil || port == nil)
    help(argv[0]);

  /* Dialing with host */
  conn = dial(host, port);
  if(conn == -1)
    panic("dial()");
  /* Configuring SIGINT interruption before stablishing connection */
  signal(SIGINT, sighandler);

  printf("connected\n");
  /* Handle connection */
  handle(conn);
  /* Close and restore terminal */
  close(conn);
  restore();
}

void
panic(const char *err)
{
  perror(err);
  exit(1);
}
  
int
dial(char *host, char *port)
{
  int n, conn;
  struct addrinfo addr = {
    0, AF_INET, SOCK_STREAM, 0,
    0, nil, nil, nil
  };
  struct addrinfo *res, *p;
  /* Getting address info */
  n = getaddrinfo(host, port, &addr, &res);
  if (n != 0)
    return -1;
  for(p = res; p != nil; p = p->ai_next){
    conn = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if(conn != -1)
      if(connect(conn, p->ai_addr, p->ai_addrlen) != -1)
        break; // good address 
    conn = -1;
  }
  freeaddrinfo(res);
  return conn;
}

void
makeraw(int fd)
{
  /* Get terminal attributes */
  tcgetattr(fd, &term);
  ancterm = term;
  /* Make it raw for pty connection */
  cfmakeraw(&term);
  /* Set attributes */
  tcsetattr(fd, TCSANOW, &term);
}

void
handle(int conn)
{
  int n, i;
  char buf;
  fd_set fds, readfds;

  makeraw(STDIN_FILENO);

  /* Creating descriptor event handler */
  FD_ZERO(&fds);
  FD_SET(conn, &fds);
  FD_SET(STDIN_FILENO, &fds);

  readfds = fds;

  int fdi, fdo;
  for(;;){
    fds = readfds;
    n = select(FD_SETSIZE, &fds, nil, nil, nil);
    if(n == -1)
      break; // error
    if(n == 0)
      continue; // closed
    /* If conn event */
    if(FD_ISSET(conn, &fds))
      fdi = conn, fdo = STDOUT_FILENO;
    else
      fdi = STDIN_FILENO, fdo = conn;
    n = read(fdi, &buf, 1);
    if(n < 1)
      break;
    n = write(fdo, &buf, 1);
    if(n < 1)
      break;
  }
}
