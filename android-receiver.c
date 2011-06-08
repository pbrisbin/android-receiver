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

/* separates message fields */
#define TOK  "/"

/* message types */
#define RING "RING"
#define SMS  "SMS"
#define MMS  "MMS"
#define PING "PING"

/* string formats */
#define FMTCALL  "  -!-  Call from %s"
#define FMTOTHER "  -!-  %s"

#define STREQ(a, b) strcmp((a),(b)) == 0


static int  portno = 10600;
static char *handler;

struct message_t {
    char *type;
    char *data;
    char *text;
};

static void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}


static void help_message() {
    fprintf(stderr, "usage: android-receiver [ --port <port> ] --handler <handler>\n\n");
    fprintf(stderr,
        "  -p, --port           the port to listen on; optional, defaults to 10600.\n"
        "  -h, --handler        an exectuable to handle the message. will be called\n"
        "                       with the formatted message as its first and only\n"
        "                       argument.\n\n");

    exit(EXIT_FAILURE);
}

static int handle_options(int argc, char *argv[]) {
    int opt, option_index = 0;
    char *token;

    static struct option opts[] = {
        { "port"   , required_argument, 0, 'p'},
        { "handler", required_argument, 0, 'h'},
        { 0        , 0                , 0, 0  }
    };

    while ((opt = getopt_long(argc, argv, "p:h:", opts, &option_index)) != -1) {

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

static struct message_t *parse_message(char *msg) {
    struct message_t *message;
    char *tok;
    int field = 0;

    message = malloc(sizeof *message);

    /* we only handle v2 message types */
    for (tok = strsep(&msg, TOK); *tok; tok = strsep(&msg, TOK)) {
        switch (++field) {
            case 4:
                if (tok)
                    message->type = strdup(tok);
                break;

            case 5:
                message->data = strdup(tok);

                if (msg) {
                    /* grab everything else */
                    if ((tok = strsep(&msg, "\0")))
                        message->text = strdup(tok);
                }

                return message;
        }
    }

    return message;
}

static void handle_message(struct message_t *message) {
    char *msg;

    if (STREQ(message->type, RING)) {
        asprintf(&msg, FMTCALL, message->text);
    }
    else if ( STREQ(message->type, SMS ) ||
              STREQ(message->type, MMS ) ||
              STREQ(message->type, PING) ) { /* test message */
        asprintf(&msg, FMTOTHER, message->text);
    }
    else {
        msg = NULL;
    }

    if (!msg)
        return;

    char *flags[] = { handler, msg, NULL };
    execvp(handler, flags);
}

static void sigchld_handler(int signum) {
    (void) signum; /* silence 'unused' warning */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    char buf[1024];

    struct message_t    *message;
    struct sockaddr_in  server, from;
    struct sigaction    sig_child;

    int          sock, n;
    unsigned int fromlen = sizeof from;

    if (handle_options(argc, argv) != 0)
        help_message();

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        error("error opening socket");

    memset(&server, '\0', sizeof server);

    server.sin_family      = AF_INET;
    server.sin_port        = htons(portno);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&server, sizeof server) < 0) 
        error("error binding to socket");

    /* listen for signals from the children we spawn */
    sig_child.sa_flags   = 0;
    sig_child.sa_handler = &sigchld_handler;
    sigemptyset(&sig_child.sa_mask);
    sigaction(SIGCHLD, &sig_child, NULL);

    while (1) {
        if ((n = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&from, &fromlen)) < 0)
            error("errorreceiving from socket");

        if (fork() == 0) {
            message = parse_message(buf);
            handle_message(message);
            exit(EXIT_SUCCESS);
        }
    }
}
