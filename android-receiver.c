#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

/* configuration */
#define PORTNO  10600
#define HANDLER "dzen-handler"

/* just the parts we care about */
struct message_t {
  char *msg_type;
  char *msg_data;
  char *msg_text;
};

char  *strndup(const char *s, size_t n);
int   asprintf(char **strp, const char *fmt, ...);
int   execvp(const char *file, char *const argv[]);
void  bzero(void *s, size_t n);
pid_t fork(void);

void             error(char *msg);
struct message_t parse_message(char *msg);
void             handle_message(struct message_t message);

/* error and die */
void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* we only handle v2 for now */
struct message_t parse_message(char *msg)
{
  struct message_t message;

  char *txt;
  char *ptr = msg;

  char delim = '/';
  int  field = 0;
  int  c = 0;
  int  i = 0;
  int  j = 0;

  while (1) {
    if (*ptr== delim)
    {
      field++;

      if (i)
        j += i + 1;

      i = c - j;

      switch(field)
      {
        case 4:
          txt = strndup(msg + j, i);
          message.msg_type = txt;
          break;
        case 5:
          txt = strndup(msg + j, i);
          message.msg_data = txt;
          break;
      }
    }

    c++;

    /* EOM */
    if (*ptr++ == '\0')
      break;
  }

  /* and the last field is the text*/
  field++;
  j  += i + 1;
  i   = c - j;
  txt = strndup(msg + j, i);
  message.msg_text = txt;

  return message;
}

/* for now we just hand off to my existing bash script */
void handle_message(struct message_t message)
{
  char *msg;

  if (strcmp(message.msg_type, "RING") == 0) {
    asprintf(&msg, "Call from %s", message.msg_text);
  }
  else if (strcmp(message.msg_type, "SMS")  == 0 ||
           strcmp(message.msg_type, "MMS")  == 0 ||
           strcmp(message.msg_type, "PING") == 0) { /* test message */
    msg = message.msg_text;
  }
  else {
    msg = NULL;
  }

  if (!msg)
    return;

  char *flags[] = { HANDLER, msg, NULL };
  execvp(HANDLER, flags);
}

int main()
{
  unsigned int fromlen;
  int          sock;
  int          length;
  int          n;

  struct message_t message;

  struct sockaddr_in server;
  struct sockaddr_in from;

  char buf[1024];

  pid_t pid;

  sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (sock < 0) 
    error("Opening socket");

  length = sizeof(server);

  bzero(&server,length);

  server.sin_family      = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port        = htons(PORTNO);

  if (bind(sock, (struct sockaddr *)&server, length) < 0) 
    error("binding");

  fromlen = sizeof(struct sockaddr_in);

  while (1) {
    n = recvfrom(sock, buf, 1024, 0, (struct sockaddr * restrict)&from, &fromlen);

    if (n < 0) 
      error("recvfrom");

    pid = fork();

    if (pid == 0) {
      message = parse_message(buf);
      handle_message(message);
    }
  }
}
