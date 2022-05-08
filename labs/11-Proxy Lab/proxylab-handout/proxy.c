#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *pxy_connection_hdr = "Proxy-Connection: close\r\n\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";
static const char *host_key = "Host";

void doit(int fd);
void read_requesthdrs(rio_t *rp, int clientfd);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, int *port, char *filepath);
void build_http_header(char *http_hdr, char *hostname, char * path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  char *port;
  printf("%s", user_agent_hdr);

  if (argc != 2) {
    fprintf(stderr, "usage %s <port>\n", argv[0]);
    exit(0);
  }
  port = argv[1];
  listenfd = Open_listenfd(port);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);
    doit(connfd);
    Close(connfd);
  }
  return 0;
}

void parse_uri(char *uri, char *hostname, int *port, char *filepath) {
  *port = 80;
  char* pos = strstr(uri, "//");
  pos = pos != NULL ? pos + 2: uri;
  char *pos2 = strstr(pos, ":");
  if(pos2 != NULL) {
    *pos2 = '\0';
    sscanf(pos, "%s", hostname);
    sscanf(pos2 + 1, "%d%s", port, filepath);
  } else {
    pos2 = strstr(pos, "/");
    if(pos2 != NULL) {
      *pos2 = '\0';
      sscanf(pos, "%s", hostname);
      *pos2 = '/';
      sscanf(pos2, "%s", filepath);
    } else {
      sscanf(pos, "%s", hostname);
    }
  }
  return;
}

void build_http_header(char *http_hdr, char *hostname, char * path, int port, rio_t *client_rio) {
  char buf[MAXLINE], request_hdr[MAXLINE], host_hdr[MAXLINE], other_hdr[MAXLINE];
  sprintf(request_hdr,requestlint_hdr_format, path);
  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
    if(strcmp(buf, endof_hdr) == 0) break;  /*EOF*/

    if(!strncasecmp(buf,host_key,strlen(host_key)))/*Host:*/
    {
      strcpy(host_hdr,buf);
      continue;
    }

    if(!strncasecmp(buf,connection_hdr,strlen(connection_hdr))
      &&!strncasecmp(buf,pxy_connection_hdr,strlen(pxy_connection_hdr))
      &&!strncasecmp(buf,user_agent_hdr,strlen(user_agent_hdr))
      &&!strncasecmp(buf,accept_hdr,strlen(accept_hdr))
      &&!strncasecmp(buf,accept_encoding_hdr,strlen(accept_encoding_hdr))
      )
      strcat(other_hdr,buf);
  }
  if(strlen(host_hdr) == 0)
    sprintf(host_hdr,host_hdr_format,hostname);

  sprintf(http_hdr, "%s%s%s%s%s%s%s%s%s",
          request_hdr,
          host_hdr,
          user_agent_hdr,
          accept_hdr,
          accept_encoding_hdr,
          connection_hdr,
          pxy_connection_hdr,
          other_hdr,
          endof_hdr
          );
  return;
}

void doit(int fd) {
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], path[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE];
  char end_server_http_header[MAXLINE];
  int clientfd;
  int port;
  rio_t rio, server_rio;

  /* read request line and headers */
  // read method, uri, version
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  parse_uri(uri, hostname, &port, path);
  build_http_header(end_server_http_header, hostname, path, port, &rio);

  clientfd = connect_endServer(hostname, port);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_readinitb(&server_rio,clientfd);
  Rio_writen(clientfd, end_server_http_header, strlen(end_server_http_header));

  size_t n;
  while((n=Rio_readlineb(&server_rio,buf,MAXLINE))!=0)
  {
    printf("proxy received %ld bytes,then send\n", n);
    Rio_writen(fd,buf, n);
  }
  Close(clientfd);
  printf("Proxy Service Done!\r\n");
}

inline int connect_endServer(char *hostname, int port) {
  char portStr[100];
  sprintf(portStr,"%d",port);
  return Open_clientfd(hostname,portStr);
}