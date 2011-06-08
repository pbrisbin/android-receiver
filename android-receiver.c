#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <getopt.h>

#define TOK  "/"

#define RING "RING"
#define SMS  "SMS"
#define MMS  "MMS"
#define PING "PING"

#define FMTCALL  "  -!-  Call from %s"
#define FMTOTHER "  -!-  %s"

/* global option vars */
int portno = 10600;
char *handler;

/* just the parts we care about */
struct message_t {
    char *msg_type;
    char *msg_data;
    char *msg_text;
};

/* error and die */
static void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/* a help message */
static void help_message() {
    fprintf(stderr, "usage: android-receiver [ --port <port> ] --handler <handler>\n\n");
    fprintf(stderr,
        "  -p, --port           the port to listen on; optional, defaults to 10600.\n"
        "  -h, --handler        an exectuable to handle the message. will be called\n"
        "                       with the formatted message as its first and only\n"
        "                       argument.\n\n");

    exit(EXIT_FAILURE);
}

/* sets handler and portno or reports failure on invalid options */
static int handle_options(int argc, char *argv[]) {
    int opt, option_index = 0;

    static struct option opts[] = {
        { "port"   , required_argument, 0, 'p'},
        { "handler", required_argument, 0, 'h'},
        { 0        , 0                , 0, 0  }
    };

    while ((opt = getopt_long(argc, argv, "p:h:", opts, &option_index)) != -1) {
        char *token;

        switch(opt) {
            case 'p':
                portno = strtol(optarg, &token, 10);
                if (*token != '\0' || portno <= 0 || portno > 65535) {
                    fprintf(stderr, "error: invalid port number\n\n");
                    return 1;
                }
                break;

            case 'h':
                handler = optarg;
                break;

            case '?':
                return 1;
            default:
                return 1;
        }
    }

    if (!handler) {
        fprintf(stderr, "error: handler is required\n\n");
        return 1;
    }

    return 0;
}

/* we only handle v2 for now */
static struct message_t *parse_message(char *msg) {
    struct message_t *message;
    char *tok;
    int field = 0;

    message = malloc(sizeof *message);

    for (tok = strsep(&msg, TOK); *tok; tok = strsep(&msg, TOK)) {
        switch (++field) {
            case 4:
                if (tok)
                    message->msg_type = strdup(tok);
                break;

            case 5:
                message->msg_data = strdup(tok);

                if (msg) {
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
static void handle_message(struct message_t *message) {
    char *msg;

    if (strcmp(message->msg_type, RING) == 0) {
        asprintf(&msg, FMTCALL, message->msg_text);
    }
    else if (strcmp(message->msg_type, SMS)  == 0 ||
             strcmp(message->msg_type, MMS)  == 0 ||
             strcmp(message->msg_type, PING) == 0) { /* test message */
        asprintf(&msg, FMTOTHER, message->msg_text);
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
static void sigchld_handler(int signum) {
    (void) signum; /* silence unused warning */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    unsigned int fromlen;
    int          sock, length, n;

    struct message_t *message;
    struct sockaddr_in server, from;
    struct sigaction sig_child;

    char buf[1024];
    pid_t pid;

    /* parse for --port and --handler */
    if (handle_options(argc, argv) != 0) {
        help_message();
    }

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

    while (1) {
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
