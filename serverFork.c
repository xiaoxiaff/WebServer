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
void generateResponse(int sock, const RequestHeader header);

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
  memset(header->uri, 0, sizeof(header->uri));
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
  FILE * f = fopen(header.uri, "rb");

  if (f)
  {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length+1);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }
  else {
    fileNotExist(sock, header.uri);
    return;
  }

  generateResponse(sock, header);
  
  if (buffer)
  {
    buffer[length] = '\0';
    printf("buffer: %s\n", buffer);
    send(sock, buffer, strlen(buffer), 0);
  }
}

void fileNotExist(int sock, const char* file)
{
  printf("fail\n");
  send(sock, STATUS_404, strlen(STATUS_404), 0);
  send(sock, ERROR_404, strlen(ERROR_404), 0);
  printf("Error: %s file doesn't Exist!\n", file);
}

void generateResponse(int sock, const RequestHeader header)
{
  char buffer[512];
  printf("generate response\n");

  int offset = 0;
  memcpy(buffer, STATUS_200, strlen(STATUS_200));
  offset += strlen(STATUS_200);

  char *connection = "Connection: close\r\n";
  memcpy(buffer+offset, connection, strlen(connection));
  offset += strlen(connection);

  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  char outstr[200];
  strftime(outstr, sizeof(outstr), "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", &tm);
  printf("Time is: [%s]\n", outstr);
  memcpy(buffer+offset, outstr, strlen(outstr));
  offset += strlen(outstr);

  char *server = "Server: yukaiAndrea/1.0\r\n";
  memcpy(buffer+offset, server, strlen(server));
  offset += strlen(server);

  char *contentType =  "Content-Type: text/html\r\n\0";
  memcpy(buffer+offset, contentType, strlen(contentType));
  offset += strlen(contentType);

  printf("buffer:%s\n", buffer);
  send(sock, buffer, strlen(buffer), 0);
}