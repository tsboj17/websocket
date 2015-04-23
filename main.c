#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <libwebsockets.h>
#include <pthread.h>

struct linked_list {
   char* val;
   int size_of_val;
   struct linked_list* next;
};

typedef struct linked_list element;

element* head = NULL;
int force_exit = 0;
pthread_mutex_t lock;

char* add_char_to_string(char* string, int size) {
    int i;
    char* ret = (char*)malloc(size+1);
    for (i=0; i<size; ++i) {
        ret[i]=string[i];
    }
    ret[size]=='\0';
    return ret;
}

element* push_to_list(element* head, char* val, size_t size) {
    if (head == NULL) { //if the linked list is empty
        head = (element*)malloc(sizeof(element));
        head->next=NULL;
        head->val=add_char_to_string(val, size);
        head->size_of_val=size;
        return head;
    }
    element* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = (element*) malloc(sizeof(element));
    current->next->val = add_char_to_string(val, size);
    current->next->size_of_val = size;
    current->next->next = NULL;
    return head;
}

void self_gc(element* head) {
    element *current = head;
    while (current != NULL) {
        element *next = current->next;
        free(current->val);
        free(current);
        current = next;
    }
    head = NULL;
}


static int
my_protocol_callback(struct libwebsocket_context *context,
		     struct libwebsocket *wsi,
		     enum libwebsocket_callback_reasons reason,
             void *user, void *in, size_t len)
{
	int n, m, i;
	unsigned char* buf;
	element* current;


	switch (reason) {

	case LWS_CALLBACK_ESTABLISHED:
	        printf("New Connection\n");
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		break;

	case LWS_CALLBACK_RECEIVE:

        pthread_mutex_lock(&lock);

        current = head;
        while (current != NULL) {
            buf = (unsigned char*) malloc(LWS_SEND_BUFFER_PRE_PADDING + current->size_of_val + LWS_SEND_BUFFER_POST_PADDING);
            for (i=0; i < current->size_of_val; ++i) {
                buf[LWS_SEND_BUFFER_PRE_PADDING + i ] = current->val[i];
            }
            libwebsocket_write(wsi, &buf[LWS_SEND_BUFFER_PRE_PADDING], current->size_of_val, LWS_WRITE_TEXT);
            free(buf);
            current = current->next;
        }

        pthread_mutex_unlock(&lock);

        break;

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		break;

	default:
		break;
	}

	return 0;
}

// list of supported protocols and callbacks
static struct libwebsocket_protocols protocols[] = {
	{
		"my-protocol",
		my_protocol_callback,
		30,
		10,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

// signaling
void sighandler(int sig) {
	force_exit = 1;
}

//Websocket worker threading
void *work_websocket(void* ptr)
{
    struct libwebsocket_context *context = NULL;
	int n = 0;
	int opts = 0;
	const char *iface = NULL;
	int syslog_options = LOG_PID | LOG_PERROR;
	unsigned int oldus = 0;
	struct lws_context_creation_info info;

	int debug_level = 7;

	memset(&info, 0, sizeof info);
	info.port = 9999;


	signal(SIGINT, sighandler);

	// we will only try to log things according to our debug_level
	setlogmask(LOG_UPTO (LOG_DEBUG));
	openlog("lwsts", syslog_options, LOG_DAEMON);

	// tell the library what debug level to emit and to send it to syslog
	lws_set_log_level(debug_level, lwsl_emit_syslog);

	info.iface = iface;
	info.protocols = protocols;
	info.extensions = libwebsocket_get_internal_extensions();
    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;

	info.gid = -1;
	info.uid = -1;
	info.options = opts;

	context = libwebsocket_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return NULL;
	}

	n = 0;
	while (n >= 0 && !force_exit) {
		n = libwebsocket_service(context, 50);
	};


	lwsl_notice("libwebsockets-test-server exited cleanly\n");

	closelog();

	libwebsocket_context_destroy(context);
	return NULL;
}

void write_to_file(char* path, char* str) {
  FILE *out = fopen(path, "a");
  fprintf(out, "%s", str);
  fclose(out);
}

int main(int argc, char **argv)
{
    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "mutex init failed\n");
        return 1;
    }

    pthread_t thread_websocket;
    if(pthread_create(&thread_websocket, NULL, work_websocket, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    int line_size;
    size_t size;
    char* path = "/media/tsboj/Munka/GSoC/file.out";

    while (!force_exit) {
        char* line = NULL;
        line_size = getline(&line, &size, stdin);
        if (line_size == -1) {

        } else {
            pthread_mutex_lock(&lock);
            head = push_to_list(head, line, line_size);
            pthread_mutex_unlock(&lock);
            write_to_file(path, line);
        }
        free(line); //the getline() function use malloc so we need free()
	}

    self_gc(head);
    pthread_mutex_destroy(&lock);
    (void) pthread_join(thread_websocket, NULL);
	return 0;
}
