/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

#define maxURILength 2000
#define STATUS_200 "HTTP/1.1 200 OK\r\n"
#define STATUS_404 "HTTP/1.1 404 Not Found\r\n\r\n"

#define ERROR_404 "<h1>Error 404: File Not Found</h1> <hr><p>File doesn't exist at the server</p>"

typedef enum
{
  html, txt, jpeg, gif, jpg
}
format;

/*  returns 1 iff str ends with suffix  */
int str_ends_with(const char * str, const char * suffix) {
  printf("f and s: %s, %s\n", str, suffix);
  if( str == NULL || suffix == NULL )
    return 0;

  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);

  if(suffix_len > str_len)
    return 0;

  return 0 == strncmp( str + str_len - suffix_len, suffix, suffix_len );
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct RequestHeader {
   char method[5];
   char uri[maxURILength];
   char version[9];
} RequestHeader;

void dostuff(int, RequestHeader*); /* function prototype */
void serveRequest(int sock, const RequestHeader header);
void fileNotExist(int sock, const char* file);
void generateResponse(int sock, format f, size_t filesize);
format getFileFormat(const char *filename);

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD
     struct RequestHeader header;

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd, &header);
             serveRequest(newsockfd, header);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock, RequestHeader* header)
{
  int n;
  char buffer[512];

  bzero(buffer,512);

  n = read(sock, buffer, 512);
  if (n < 0) error("ERROR reading from socket");
  const char s[2] = "\n";
  const char split[2] = " ";
  char *token;
  char *item;
  
  printf("buffer: %s\n", buffer);
   /* get the first token */
  token = strtok(buffer, s);
  memset(header->uri, '\0', sizeof(header->uri));
  if (item = strtok(token, split))
    strcpy(header->method, item);
  if (item = strtok(NULL, split))
    strcpy(header->uri, item+1);
  if (item = strtok(NULL, split))
    strcpy(header->version, item);

  printf("%s,%s,%s\n", header->method, header->uri, header->version );
}

/******** SERVEREQUEST() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void serveRequest(int sock, const RequestHeader header)
{
  printf("%s\n", header.uri);
  if(header.uri[0]=='\0') {
    fileNotExist(sock, header.uri);
    return;
  }
  //write header
  char * buffer = 0;
  long length;
  size_t fileLength = 0;
  FILE * f = fopen(header.uri, "rb");

  if (f == NULL) {
    fileNotExist(sock, header.uri);
    return;
  }

  if (fseek (f, 0L, SEEK_END) == 0)
  {
    length = ftell (f);
    buffer = malloc (sizeof(char)*(length+1));
    if (fseek (f, 0L, SEEK_SET) != 0) {
      printf("file size error");
      fileNotExist(sock, header.uri);
      return;
    }
    if (buffer)
    {
      fileLength = fread (buffer, 1, length, f);
    }
    fclose (f);
  }
  else {
  }

  format fmat = getFileFormat(header.uri);

  printf("fmat:%d, length:%d, fileLength:%d\n", fmat, length, fileLength);

  //size_t fileLength = fread(buffer, sizeof(char), length, f);

  buffer[fileLength] = '\0';

  generateResponse(sock, fmat, fileLength);
  
  printf("buffer: %s\n", buffer);
  send(sock, buffer, fileLength, 0);
}

void fileNotExist(int sock, const char* file)
{
  printf("fail\n");
  send(sock, STATUS_404, strlen(STATUS_404), 0);
  send(sock, ERROR_404, strlen(ERROR_404), 0);
  printf("Error: %s file doesn't Exist!\n", file);
}

void generateResponse(int sock, format f, size_t filesize)
{
  char buffer[512];
  printf("generate response\n");

  int offset = 0;
  memcpy(buffer, STATUS_200, strlen(STATUS_200));
  offset += strlen(STATUS_200);
  printf("before connection close\n");

  char *connection = "Connection: close\r\n";
  memcpy(buffer+offset, connection, strlen(connection));
  offset += strlen(connection);

  printf("before time\n");
  time_t now = time(0);
  struct tm *mytime = localtime(&now); 
  printf("before allocate\n");
  char outstr[100];
  printf("before strftime\n");
  strftime(outstr, sizeof(outstr), "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", mytime);
  printf("Time is: [%s]\n", outstr);
  memcpy(buffer+offset, outstr, strlen(outstr));
  offset += strlen(outstr);

  char *server = "Server: yukaiAndrea/1.0\r\n";
  memcpy(buffer+offset, server, strlen(server));
  offset += strlen(server);
  printf("before content length\n");

  char contentLength[50] = "Content-Length: ";  
  char len[10];
  sprintf (len, "%d", (unsigned int)filesize);
  strcat(contentLength, len);
  strcat(contentLength, "\r\n");

  memcpy(buffer+offset, contentLength, strlen(contentLength));
  offset += strlen(contentLength);

  printf("before content type\n");

  char *contentType;
  if (f == html)  contentType =  "Content-Type: text/html\r\n";
  if (f == txt)  contentType =  "Content-Type: text/plain\r\n";
  if (f == jpeg)  contentType =  "Content-Type: image/jpeg\r\n";
  if (f == jpg)  contentType =  "Content-Type: image/jpg\r\n";
  if (f == gif)  contentType =  "Content-Type: image/gif\r\n";
  memcpy(buffer+offset, contentType, strlen(contentType));
  offset += strlen(contentType);

  memcpy(buffer+offset, "\r\n\0", 3);

  printf("buffer:%s\n", buffer);
  send(sock, buffer, strlen(buffer), 0);
}

format getFileFormat(const char *filename)
{
  if (str_ends_with(filename, ".html") != NULL) return html;
  if (str_ends_with(filename, ".txt") != NULL) return txt;
  if (str_ends_with(filename, ".jpeg") != NULL) return jpeg;
  if (str_ends_with(filename, ".gif") != NULL) return gif;
  if (str_ends_with(filename, ".jpg") != NULL) return jpg;

  return txt;
}