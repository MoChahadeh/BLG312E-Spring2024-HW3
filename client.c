#include "blg312e.h" // this header file already includes all the necessary libraries


// a data structure to hold the args and to pass them to the thread function
struct Args {

    char* host;
    int port;
    char* filename;

} typedef Args;

void clientSend(int fd, char *filename)
{
    char buf[MAXLINE];
    char hostname[MAXLINE];

    Gethostname(hostname, MAXLINE);

    /* Form and send the HTTP request */
    snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\n", filename);
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "host: %s\n\r\n", hostname);
    Rio_writen(fd, buf, strlen(buf));
}

  
/*
 * Read the HTTP response and print it out
 */
void clientPrint(int fd)
{
  rio_t rio;
  char buf[MAXBUF];  
  int length = 0;
  int n;
  
  Rio_readinitb(&rio, fd);

  /* Read and display the HTTP Header */
  n = Rio_readlineb(&rio, buf, MAXBUF);
  while (strcmp(buf, "\r\n") && (n > 0)) {
    printf("Header: %s", buf);
    n = Rio_readlineb(&rio, buf, MAXBUF);

    /* If you want to look for certain HTTP tags... */
    if (sscanf(buf, "Content-Length: %d ", &length) == 1) {
      printf("Length = %d\n", length);
    }
  }

  /* Read and display the HTTP Body */
  n = Rio_readlineb(&rio, buf, MAXBUF);
  while (n > 0) {
    printf("%s", buf);
    n = Rio_readlineb(&rio, buf, MAXBUF);
  }
}

void* thread_function(void* args_ptr) {

  Args* args = (Args*) args_ptr;

  // open a connection to the specified host and port in the args
  int clientfd = Open_clientfd(args->host, args->port);

  clientSend(clientfd, args->filename);
  clientPrint(clientfd);

  Close(clientfd);
  return NULL;
}

int main(int argc, char *argv[])
{

    if (argc != 4) {
    fprintf(stderr, "Usage: %s <host> <port> <filename>\n", argv[0]);
    exit(1);
    }

    Args args = {argv[1], atoi(argv[2]), argv[3]};

    int num_of_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_of_threads == -1) {
        perror("sysconf");
        exit(EXIT_FAILURE);
    }

    pthread_t threads[num_of_threads];
    for (int i = 0; i < num_of_threads; i++) {
        if(pthread_create(&threads[i], NULL, thread_function, &args) != 0) {
          perror("pthread_create");
          exit(1);
        }
    }

    for (int i = 0; i < num_of_threads; i++) {
        printf("Waiting for thread %d to finish\n", i);
        pthread_join(threads[i], NULL);
    }
    
    exit(0);
}
