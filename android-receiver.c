#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <getopt.h>
#include <errno.h>

#define DEBUG    0
#define MAXBUF   1024
#define TOK      "/"
#define FMTCALL  "  -!-  Call from %s"
#define FMTOTHER "  -!-  %s"

#define STREQ(a, b)     strcmp((a),(b)) == 0
#define STRDUP(a, b)    if ((b)) a = strdup((b))

static int  portno = 10600;
static char *handler;

enum etype {
    Ring,
    SMS,
    MMS,
    Battery,
    Ping,
    Unknown
};

struct message_t {
    int         version;
    char *      device_id;
    char *      notification_id;
    enum etype  event_type;
    char *      data;
    char *      event_contents;
};

static void error(char *msg) { /* {{{ */
    perror(msg);
    exit(EXIT_FAILURE);
}
/* }}} */

static void help_message() { /* {{{ */
    fprintf(stderr, "usage: android-receiver [ -p <port> ] -h <handler>                          \n"
                    "    -p, --port     the port to listen on. optional, defaults to 10600.      \n"
                    "    -h, --handler  an executable to handle the message. will be called with \n"
                    "                   the formatted message as its first and only argument.    \n");

    exit(EXIT_FAILURE);
}
/* }}} */

static int handle_options(int argc, char *argv[]) { /* {{{ */
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
/* }}} */

static struct message_t *parse_message(char *msg) { /* {{{ */
    struct message_t *message;
    char *tmp;
    char *tok;
    int field = 0;

    message = malloc(sizeof *message);

    /* v1:        device_id / notification_id / event_type /        event_contents */
    /* v2: "v2" / device_id / notification_id / event_type / data / event_contents */
    for (tok = strsep(&msg, TOK); ++field <= 5; tok = strsep(&msg, TOK)) {
        switch (field) {
            case 1:
                if (tok && STREQ(tok, "v2"))
                    message->version = 2;
                else {
                    /* rebuild a v1 msg to start parsing at field 2 */
                    message->version = 1;
                    tmp = strdup(msg);
                    strcat(tok, TOK);
                    strcat(tok, tmp);
                    msg = tok;
                }
                break;

            case 2: STRDUP(message->device_id, tok);       break;
            case 3: STRDUP(message->notification_id, tok); break;

            case 4:
                if      (STREQ(tok, "RING"))    message->event_type = Ring;
                else if (STREQ(tok, "SMS"))     message->event_type = SMS;
                else if (STREQ(tok, "MMS"))     message->event_type = MMS;
                else if (STREQ(tok, "BATTERY")) message->event_type = Battery;
                else if (STREQ(tok, "PING"))    message->event_type = Ping;
                else                            message->event_type = Unknown;

                if (message->version == 1) {
                    /* for v1, grab everything else and return */
                    STRDUP(message->event_contents, msg);
                    return message;
                }
                break;

            case 5:
                STRDUP(message->data, tok);
                STRDUP(message->event_contents, msg);
                return message;
        }
    }

    return message;
}
/* }}} */

static void handle_message(struct message_t *message) { /* {{{ */
    char *msg;

    switch (message->event_type) {
        case Ring:
            asprintf(&msg, FMTCALL, message->event_contents);
            break;

        case SMS:
        case MMS:
        case Battery:
        case Ping: /* todo: other type-specific formats */
            asprintf(&msg, FMTOTHER, message->event_contents);
            break;

        default: msg = NULL;
    }

    if (!msg)
        return;

    char *flags[] = { handler, msg, (char *)NULL };
    execvp(handler, flags);
}
/* }}} */

int main(int argc, char *argv[]) { /* {{{ */
    char buf[MAXBUF];

    struct message_t    *message;
    struct sockaddr_in  server, from;

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

    while (1) {
        while ((n = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&from, &fromlen)) < 0 && errno == EINTR)
            ;

        if (n < 0)
            error("error receiving from socket");

        if (DEBUG)
            printf("message received: %s\n", buf);

        /* double-fork taken from dzen2/util.c; avoids zombie processes
         * without requiring a signal handler. */
        if (fork() == 0) {
            if (fork() == 0) {
                message = parse_message(buf);
                handle_message(message);
                exit(EXIT_SUCCESS);
            }
            exit(EXIT_SUCCESS);
        }
        wait(0);
    }
}
/* }}} */
