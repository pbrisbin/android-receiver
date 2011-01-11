#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

/* configuration */
#define PORTNO  10600
#define HANDLER "dzen-handler"

/* just the parts we care about */
struct message_t
{
    char *msg_type;
    char *msg_data;
    char *msg_text;
};

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

    char *ptr = msg;

    char delim = '/';

    int field = 0;

    int  c = 0; /* n position in the overall string       */
    int  i = 0; /* accumulated length of last seen field  */
    int  j = 0; /* n position of start of last seen field */

    while (1) {
        if (*ptr== delim)
        {
            field++;

            /* there's a possibility of slashes in the sixth field so we'll
             * parse up to 5 and let the rest be picked up after the loop */
            if (field <= 5)
            {
                if (i) /* these three lines made my head hurt */
                    j += i + 1;

                i = c - j;

                switch(field)
                {
                    case 4:
                        message.msg_type = strndup(msg + j, i);
                        break;
                    case 5:
                        message.msg_data = strndup(msg + j, i);
                        break;
                }
            }
        }

        c++;

        /* EOM */
        if (*ptr++ == '\0')
            break;
    }

    j += i + 1;
    i  = c - j;

    /* the last field is the text*/
    message.msg_text = strndup(msg + j, i);

    return message;
}

/* for now we just hand off to my existing bash script */
void handle_message(struct message_t message)
{
    char *msg;

    if (strcmp(message.msg_type, "RING") == 0)
    {
        asprintf(&msg, "Call from %s", message.msg_text);
    }
    else if (strcmp(message.msg_type, "SMS")  == 0 ||
             strcmp(message.msg_type, "MMS")  == 0 ||
             strcmp(message.msg_type, "PING") == 0) /* test message */
    {
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
        error("opening socket");

    length = sizeof(server);

    memset(&server, '\0', length);

    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port        = htons(PORTNO);

    if (bind(sock, (struct sockaddr *)&server, length) < 0) 
        error("binding to socket");

    fromlen = sizeof(struct sockaddr_in);

    while (1)
    {
        n = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&from, &fromlen);

        if (n < 0) 
            error("recveiving from socket");

        pid = fork();

        if (pid == 0) {
            message = parse_message(buf);
            handle_message(message);
        }
    }
}
