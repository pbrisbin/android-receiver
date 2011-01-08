#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

int   execvp(const char *file, char *const argv[]);
void  bzero(void *s, size_t n);
pid_t fork(void);

void error(char *msg);
void handle_message(char *msg);

/* error and die */
void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* for now we just hand off to my existing bash script */
void handle_message(char *msg)
{
  char *app = "dzen-handler";
  char *flags[] = {app, msg, NULL};

  printf("executing handler: %s %s\n", flags[0], flags[1]);
  execvp(app, flags);
}

/* connect to <port> and listen for messages */
int main(int argc, char *argv[])
{
  unsigned int fromlen;
  int          sock;
  int          length;
  int          n;

  struct sockaddr_in server;
  struct sockaddr_in from;

  char buf[1024];

  pid_t pid;

  if (argc != 2) {
    fprintf(stderr, "argc was %i\n", argc);
    fprintf(stderr, "saw %s, %s, %s\n", argv[0], argv[1], argv[2]);
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return(EXIT_FAILURE);
  }
   
  sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (sock < 0) 
    error("Opening socket");

  length = sizeof(server);

  bzero(&server,length);

  server.sin_family      = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port        = htons(atoi(argv[1]));

  if (bind(sock, (struct sockaddr *)&server, length) < 0) 
    error("binding");

  fromlen = sizeof(struct sockaddr_in);

  while (1) {
    n = recvfrom(sock, buf, 1024, 0, (struct sockaddr * restrict)&from, &fromlen);

    if (n < 0) 
      error("recvfrom");

    pid = fork();

    if (pid == 0)
      handle_message(buf);
  }
}
