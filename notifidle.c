#define _GNU_SOURCE
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdarg.h>

#include <libnotify/notify.h>

#define DEBUG 1
#ifdef DEBUG
#  define _debug(fmt) printf(fmt)
#  define debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#  define _debug(unused)
#  define debug(unused, ...)
#endif

struct globals {
  char * host;
  char * user;
  char * pass;
  char * mailbox;
  unsigned int port;
  int fd;
} globals = {
  .host = "mail.megacorp.com",
  .user = "johnny",
  .pass = "hunter2",
  .mailbox = "INBOX",
  .port = 143,
  .fd = -1,
};

typedef struct ni_line {
  char * data;
} ni_line;

static void handle_login_failure(ssize_t buf_len, char * buffer){
  /* XXX */
}

static void handle_examine_failure(ssize_t buf_len, char * buffer){
  /* ni_line response_lines[] = ni_parse_response_lines(buf_len, buffer); */
}

static void ni_imap_cmd(unsigned int server, unsigned short need_tag, void (callback)(ssize_t, char *), const char *fmt, ...){
  char *command = NULL;
  static unsigned int command_nr = 0;
  va_list ap;
  va_start(ap, fmt);
  va_end(ap);

  vasprintf(&command, fmt, ap);

  if(need_tag){
    asprintf(&command, "%04d %s\n", ++command_nr, command);
  } else {
    asprintf(&command, "%s\n", command);
  }

  ssize_t sent = send(server, command, strlen(command), 0);
  free(command);

  unsigned int recv_size = 128;
  char * reply = malloc(recv_size);
  ssize_t received_bytes;
  while( (received_bytes = recv(server, reply, recv_size, MSG_PEEK)) == recv_size ){
    recv_size *= 1.5;
    reply = realloc(reply, recv_size);
  }

  if((received_bytes = recv(server, reply, recv_size, 0)) == -1){
    perror("recv");
    return;
  }
  reply[received_bytes] = '\0';

  debug("sent %d, received %d [%s]\n", (int)sent, (int)received_bytes, reply);

  /* XXX parse OK/NO response */
  if(callback){
    callback(received_bytes, reply);
  }

  free(reply);
}

static void ni_login(unsigned int server){
  unsigned int banner_size = 128;
  char * banner = malloc(banner_size);
  ssize_t received;
  while( (received = recv(server, banner, banner_size, MSG_PEEK)) == banner_size ){
    banner_size *= 1.5;
    banner = realloc(banner, banner_size);
  }
  if((received = recv(server, banner, banner_size, 0)) == -1){
    perror("can't get banner");
    return;
  }
  banner[received] = '\0';
  debug("banner [%s]\n", banner);
  free(banner);

  ni_imap_cmd(server, 1, handle_login_failure, "LOGIN %s %s", globals.user, globals.pass);
  ni_imap_cmd(server, 1, handle_examine_failure, "EXAMINE %s", globals.mailbox);
}

static unsigned long parse_message_id(char * buffer){
  unsigned long toret = 0;

  sscanf(buffer, "* %lu EXISTS", &toret);

  return toret;
}

static void parse_headers(ssize_t buf_len, char * buffer){
  /* TODO: really parse headers */
  /* XXX: 20: not good */
  char * new_buffer = malloc(buf_len + 20),
       * p = buffer,
       * np = new_buffer;

  while(*p++){
    switch(*p){
      case '<':
        *np++ = '&';
        *np++ = 'l';
        *np++ = 't';
        *np++ = ';';
        break;
      case '>':
        *np++ = '&';
        *np++ = 'g';
        *np++ = 't';
        *np++ = ';';
        break;
      default:
        *np++ = *p;
        break;
    }
  }

  NotifyNotification * note = notify_notification_new(
    "<span color=\"yellow\">NEW MAIL</span>",
    new_buffer,
    "/usr/share/icons/gnome/48x48/stock/net/stock_mail-open.png"
  );
  free(new_buffer);

  notify_notification_set_timeout(note, 8000);
  notify_notification_show(note, NULL);
}

static void handle_message(unsigned int server, unsigned long message_id){
  ni_imap_cmd(server, 1, parse_headers, "FETCH %lu BODY[HEADER.FIELDS (From To Cc Subject List-Id)]", message_id);
}

static void ni_idle(unsigned int server){
  ni_imap_cmd(server, 1, NULL, "IDLE");

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(server, &read_fds);

  int something;
  unsigned int recv_size = 1024;
  char * reply = malloc(recv_size);
  ssize_t received;

  while( (something = select(server + 1, &read_fds, NULL, NULL, NULL)) != -1 ){
    if(!FD_ISSET(server, &read_fds))
      continue;

    while( (received = recv(server, reply, recv_size, MSG_PEEK)) == recv_size ){
      recv_size *= 1.5;
      reply = realloc(reply, recv_size);
    }

    if((received = recv(server, reply, recv_size, 0)) == -1){
      perror("recv");
      return;
    }
    reply[received] = '\0';

    debug("broke idle [%s]\n", reply);

    unsigned long message_id = parse_message_id(reply);
    if(!message_id)
      continue;

    debug("message id [%lu]\n", message_id);

    ni_imap_cmd(server, 0, NULL, "DONE");

    handle_message(server, message_id);

    ni_imap_cmd(server, 1, NULL, "IDLE");
  }

  free(reply);
}

static void notifidle(unsigned int server){
  notify_init("notifidle");
  ni_login(server);
  ni_idle(server);
}

int main (int argc, char * const argv[]){
  int opt, svc = 143;

  while ( (opt = getopt(argc, argv, "h:P:u:p:m:")) != -1 ){
    switch (opt){
      case 'h':
        globals.host = strdup(optarg);
        break;
      case 'P':
        svc = atoi(optarg);
        break;
      case 'u':
        globals.user = strdup(optarg);
        memset(optarg, '*', strlen(optarg));
        break;
      case 'p':
        globals.pass = strdup(optarg);
        memset(optarg, '*', strlen(optarg));
        break;
      case 'm':
        globals.mailbox = strdup(optarg);
        break;
      case 'f':
        globals.fd = atoi(optarg);
        break;
      default:
        fprintf(stderr, "wtf?  use with -h host -P port -u user -p pass -m mailbox\n");
        exit(1);
        break;
    }
  }

  if(!globals.user || !globals.pass){
    fprintf(stderr, "missing one of -u user -p pass\n");
    exit(1);
  }

  if (globals.fd == -1){
    struct hostent *he;
    struct sockaddr_in servr = {
      .sin_family = AF_INET,
      .sin_port = htons(svc),
    };

    if ( (he = gethostbyname(globals.host)) == NULL ){
      fprintf(stderr, "gethostbyname failed\n");
      exit(1);
    }

    memcpy(&servr.sin_addr, he->h_addr_list[0], he->h_length);

    unsigned int s = socket(AF_INET, SOCK_STREAM, 0);

    debug("connecting to %s:%u...\n", globals.host, svc);
    if (connect(s, (struct sockaddr *) &servr, sizeof(servr))){
      fprintf(stderr, "connect failed\n");
      exit(1);
    }
  } else {
  }

  debug("connected on fd %u\n", globals.fd);
  notifidle(globals.fd);

  return 0;
}

/* vim: set ts=2 sw=2 et */
