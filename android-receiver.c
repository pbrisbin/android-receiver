#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>

/* global option vars */
unsigned int portno = 10600;
char *handler;

/* just the parts we care about */
struct message_t
{
    char *msg_type;
    char *msg_data;
    char *msg_text;
};

/* error and die */
static void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/* sets handler and portno or reports failure on invalid options */
static void handle_options(int argc, char *argv[])
{
    switch (argc)
    {
        case 3: /* port can be omitted */
            if (strcmp(argv[1], "--handler") == 0)
            {
                handler = argv[2];
                return;
            }
            break;

        case 5:
            if (strcmp(argv[1], "--port") == 0)
            {
                portno = atoi(argv[2]);

                if (strcmp(argv[3], "--handler") == 0)
                {
                    handler = argv[4];
                    return;
                }
            }
            break;
    }

    /* if we get here, we fail */
    printf( "usage: %s [ --port <port> ] --handler <handler>\n", argv[0] );
    exit(EXIT_FAILURE);
}

/* we only handle v2 for now */
static struct message_t *parse_message(char *msg)
{
    struct message_t *message;
    char *tok;
    int field = 0;

    message = calloc(1, sizeof *message);

    for (tok = strsep(&msg, "/"); *tok; tok = strsep(&msg, "/"))
    {
        switch (++field)
        {
            case 4:
                if (tok)
                    message->msg_type = strdup(tok);
                break;

            case 5:
                message->msg_data = strdup(tok);

                if (msg)
                {
                    /* grab everything else */
                    tok = strsep(&msg, "\0");
                    if (tok)
                        message->msg_text = strdup(tok);
                }

                return message;
        }
    }

    return message;
}

/* for now we just hand off to my existing bash script */
static void handle_message(struct message_t *message)
{
    char *msg;

    if (strcmp(message->msg_type, "RING") == 0)
    {
        asprintf(&msg, "  -!-  Call from %s", message->msg_text);
    }
    else if (strcmp(message->msg_type, "SMS")  == 0 ||
             strcmp(message->msg_type, "MMS")  == 0 ||
             strcmp(message->msg_type, "PING") == 0) /* test message */
    {
        asprintf(&msg, "  -!-  %s", message->msg_text);
    }
    else {
        msg = NULL;
    }

    if (!msg)
        return;

    char *flags[] = { handler, msg, NULL };
    execvp(handler, flags);
}

/* signal handler for the forked handler processes */
static void sigchld_handler(int signum)
{
    (void) signum; /* silence unused warning */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[])
{
    unsigned int fromlen;
    int          sock, length, n;

    struct message_t *message;
    struct sockaddr_in server, from;
    struct sigaction sig_child;

    char buf[1024];
    pid_t pid;

    /* parse for --port and --handler */
    handle_options(argc, argv);

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) 
        error("opening socket");

    length = sizeof(server);

    memset(&server, '\0', length);

    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port        = htons(portno);

    if (bind(sock, (struct sockaddr *)&server, length) < 0) 
        error("binding to socket");

    fromlen = sizeof(struct sockaddr_in);

    /* listen for signals from the children we spawn */
    sig_child.sa_handler = &sigchld_handler;
    sigemptyset(&sig_child.sa_mask);
    sig_child.sa_flags = 0;
    sigaction(SIGCHLD, &sig_child, NULL);

    while (1)
    {
        n = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&from, &fromlen);

        if (n < 0) 
            error("receiving from socket");

        pid = fork();

        if (pid == 0) {
            message = parse_message(buf);
            handle_message(message);
            exit(EXIT_SUCCESS);
        }
    }
}
