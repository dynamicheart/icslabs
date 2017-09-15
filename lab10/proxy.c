/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Jianbang Yang, 515030910223, yangjianbang112@gmail.com
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"
#define NTHREADS 4
#define SBUFSIZE 16

/*
 * Client infomation
 */
 typedef struct{
     int clientfd;
     struct sockaddr_in clientaddr;
 } clientinfo_t;

/* 
 * Buffer pool 
 */
typedef struct{
    int *buf;
    struct sockaddr_in *addr_buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

/*
 * Global variables 
 */
FILE *log_file;
sbuf_t sbuf;
sem_t gethostbyname_mutex;
sem_t log_mutex;

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
int open_clientfd_ts(char *hostname, int port);
void clienterror(int fd, char*cause, char *errnum,
                char *shortmsg, char *longmsg);
void writelog(struct sockaddr_in *clientaddr, char *uri, int size);

void doit(clientinfo_t clientinfo);
void serve_proxy(clientinfo_t clientinfo, rio_t *rio, char *uri, char *method, char *hostname, char *pathname, int port, char *version);

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int clientfd, struct sockaddr_in clientaddr);
clientinfo_t sbuf_remove(sbuf_t *sp);
void *thread(void *vargp);



/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{

    int i, connfd, listenfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    /* Check arguments */
    if (argc != 2) {
	    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	    exit(1);
    }

    /* open listenfd */
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    
    /* open log file */
    log_file = fopen("proxy.log","a");

    /* init connection fd buffer */
    sbuf_init(&sbuf, SBUFSIZE);
    
    /* init semaphore */
    Sem_init(&gethostbyname_mutex, 0, 1);
    Sem_init(&log_mutex, 0 ,1);
    
    /* ingnore the SIGPIPE so that the process will not be terminated */
    Signal(SIGPIPE, SIG_IGN);
    
    /* create worker thread */
    for(i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }

    /* accept connection and add it to connfd buffer*/
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd, clientaddr);
    }

    sbuf_deinit(&sbuf);
    Close(listenfd);
    exit(0);
}

/*
 * thread - Worker thread
 *
 * Remove connfd from buffer and serve client
 */
void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        clientinfo_t clientinfo;
        clientinfo = sbuf_remove(&sbuf);
        doit(clientinfo);
        Close(clientinfo.clientfd);
    }
}

/* 
 * doit - handle request from client 
 */
void doit(clientinfo_t clientinfo){
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], hostname[MAXLINE], pathname[MAXLINE], version[MAXLINE];
    int port;
    rio_t rio;

    int fd = clientinfo.clientfd;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    //printf("%s\n",buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if(strcasecmp(method, "GET")){
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny proxy does not implement this method");
        return;
    }

    if(parse_uri(uri, hostname, pathname, &port) < 0){
        clienterror(fd, uri, "400", "Bad Request", 
                    "");
        return;
    }

    printf("%s %s\n", hostname, pathname);
    serve_proxy(clientinfo, &rio, uri, method, hostname, pathname, port, version);
}


/*
 * serve_proxy - Receive response from server and send it to client. Log the request if succeed 
 *
 */
void serve_proxy(clientinfo_t clientinfo, rio_t *rio, char *uri, char *method, char *hostname, char *pathname, int port, char *version){
    int serverfd, bytes;
    int tot_bytes = 0;
    char request_buf[MAXLINE],response_buf[MAXLINE], log_buffer[MAXLINE];
    rio_t response_rio;
    int clientfd = clientinfo.clientfd;
    struct sockaddr_in clientaddr = clientinfo.clientaddr;
    
    sprintf(request_buf, "%s /%s HTTP/1.0\r\n", method, pathname);
    sprintf(request_buf, "%sHost: %s\r\n\r\n", request_buf, hostname);
    printf("%s",request_buf);
    //add_requesthdrs(rio ,request_buf);

    if((serverfd = open_clientfd_ts(hostname, port)) < 0){
        return;
    }


    rio_writen(serverfd, request_buf, strlen(request_buf));
    
    rio_readinitb(&response_rio, serverfd);
    while((bytes = rio_readnb(&response_rio, response_buf, MAXLINE)) != 0){
        rio_writen(clientfd, response_buf, bytes);
        tot_bytes += bytes;
    }
    
    /* Write log */
    format_log_entry(log_buffer, &clientaddr, uri, tot_bytes);
    sprintf(log_buffer,"%s\n", log_buffer);
    printf("%s\n", log_buffer);

    P(&log_mutex);
    fprintf(log_file,"%s",log_buffer);
    fflush(log_file);
    V(&log_mutex);

    Close(serverfd);
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


/*
 * clienterror - Send the error response to client
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body, "<!DOCTYPE html>");
    sprintf(body, "%s<html><meta charset=\"UTF-8\">", body);
    sprintf(body, "%s<title>Proxy Error</title>", body);
    sprintf(body, "%s<body style=\"background-color: #fff;\">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Web Proxy</em>\r\n", body);
    
    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

/*
 *  sbuf_init - Init the buffer pool
 */
void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->addr_buf = Calloc(n, sizeof(struct sockaddr_in));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0 , n);
    Sem_init(&sp->items, 0, 0);
}

/*
 * sbuf_deinit - Deinit the buffer pool
 */
void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
    Free(sp->addr_buf);
}

/*
 * sbuf_insert - Insert new item to buffer pool
 */
void sbuf_insert(sbuf_t *sp, int clientfd, struct sockaddr_in clientaddr){
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = clientfd;
    sp->addr_buf[(++sp->rear)%(sp->n)] = clientaddr;
    V(&sp->mutex);
    V(&sp->items);
}

/*
 * sbuf_remove - Get the available item and dispatch to worker thread
 */
clientinfo_t sbuf_remove(sbuf_t *sp){
    clientinfo_t clientinfo;
    P(&sp->items);
    P(&sp->mutex);
    clientinfo.clientfd = sp->buf[(++sp->front)%(sp->n)%sp->n];
    clientinfo.clientaddr = sp->addr_buf[(++sp->front)%(sp->n)%sp->n];
    V(&sp->mutex);
    V(&sp->slots);
    return clientinfo;
}

/*
 * open_clientfd_ts - A thread-safe version of open_clientfd
 */
int open_clientfd_ts(char *hostname, int port){
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if((clientfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        return -1;
    
    /* Lock */
    P(&gethostbyname_mutex);
    
    if((hp = gethostbyname(hostname)) == NULL){
        /* Unlock */
        V(&gethostbyname_mutex);        
        return -2;
    }

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
          (char *)&serveraddr.sin_addr.s_addr, hp->h_length);

    /* Unlock */
    V(&gethostbyname_mutex);    
    
    serveraddr.sin_port = htons(port);

    if(connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}

/*
 * writelog - Log the request infomation
 */
void writelog(struct sockaddr_in *clientaddr, char *uri, int size){
    char log_entry[MAXLINE];
    
    if(size < 0){
        return;
    }

    format_log_entry(log_entry, clientaddr, uri, size);
    sprintf(log_entry,"%s\n", log_entry);
    printf("%s\n", log_entry);

    P(&log_mutex);
    fwrite(log_entry,sizeof(char), strlen(log_entry), log_file);
    fflush(log_file);
    V(&log_mutex);
}


