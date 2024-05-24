#include "blg312e.h" // this header file already includes all the necessary libraries
#include "request.h"

#define MAX_QUEUE_SIZE 10000

struct Args { 
    int port;
    int threads;
    int buffers;
} typedef Args;

// struct for the worker threads
struct Workers {
    pthread_t* threads;
    int size;
} typedef Workers;

typedef struct {
    int fds[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    sem_t sem_empty; // Semaphore to track empty slots
    sem_t sem_full;  // Semaphore to track full slots
} ConnectionQueue;

// global variables
Workers workers;
int is_shutting_down = 0;
ConnectionQueue connection_queue;



void queue_init(ConnectionQueue *q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    sem_init(&q->sem_empty, 0, MAX_QUEUE_SIZE); // Initialize to max capacity
    sem_init(&q->sem_full, 0, 0);               // Initially, no items are full
}

void queue_destroy(ConnectionQueue *q) {
    sem_destroy(&q->sem_empty);
    sem_destroy(&q->sem_full);
}

void queue_push(ConnectionQueue *q, int fd) {
    sem_wait(&q->sem_empty); // Wait until there's an empty slot
    q->fds[q->rear] = fd;
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->size++;
    sem_post(&q->sem_full); // Increment count of full slots
}

int queue_pop(ConnectionQueue *q) {
    sem_wait(&q->sem_full); // Wait until there's a full slot
    int fd = q->fds[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    sem_post(&q->sem_empty); // Increment count of empty slots
    return fd;
}


void getargs(Args* args, int argc, char *argv[])
{
    if (argc !=4) {
		fprintf(stderr, "Usage: %s <port> <threads> <buffers>\n", argv[0]);
		exit(1);
    }

    args->port = atoi(argv[1]);
    args->threads = atoi(argv[2]);
    args->buffers = atoi(argv[3]);
}


void* thread_function(void* arg) {
    while (1) {
        int connection = queue_pop(&connection_queue);

        if (is_shutting_down) {
            break;
        }

        requestHandle(connection);
        Close(connection);
    }
    return NULL;
}

void server_shutdown(int sig) {
    printf("Shutting down the server...\n");
    is_shutting_down = 1;

    // Unblock any waiting threads to allow them to exit
    for (int i = 0; i < workers.size; i++) {
        queue_push(&connection_queue, -1);  // Use -1 to signal threads to exit
    }

    for (int i = 0; i < workers.size; i++) {
        printf("Waiting for thread %d to exit...\n", i);
        pthread_join(workers.threads[i], NULL);
    }

    queue_destroy(&connection_queue);
    free(workers.threads);
    exit(0);
}


int main(int argc, char *argv[]) {
    int listenfd, connfd, clientlen;
    struct sockaddr_in clientaddr;

    Args args;
    getargs(&args, argc, argv);

    signal(SIGINT, server_shutdown);

    queue_init(&connection_queue);

    workers.size = args.threads;
    workers.threads = (pthread_t*) malloc(sizeof(pthread_t) * args.threads);

    for (int i = 0; i < args.threads; i++) {
        if (pthread_create(&workers.threads[i], NULL, thread_function, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    listenfd = Open_listenfd(args.port);
    printf("Listening on port %d\n", args.port);

    while (!is_shutting_down) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        printf("Accepted Connection fd: %d\n", connfd);

        queue_push(&connection_queue, connfd);
    }

    return 0;
}
