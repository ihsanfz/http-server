#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h> 

#define PORT 8080
#define BUFFER_SIZE 1024

char resp[] = "HTTP/1.0 200 OK\r\n"
"Server: webserver-c\r\n"
"Content-type: text/html\r\n\r\n"
"<html>Hello World</html>\r\n";


void* handle_client(void* arg) {
    int newsockfd = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    int sockn = getpeername(newsockfd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addrlen);

    int valread = read(newsockfd, buffer, BUFFER_SIZE);

    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);
    printf("[%s:%u] %s %s %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), method, version, uri);

    int valwrite = write(newsockfd, resp, strlen(resp));

    close(newsockfd);
}

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("webserver (socket)");
    }
    printf("Socket creation successful\n");

    struct sockaddr_in host_addr, client_addr;
    int host_addrlen = sizeof(host_addr);
    int client_addrlen = sizeof(client_addr);

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(PORT);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&host_addr, host_addrlen) != 0) {
        perror("webserver (bind)");
        return 1;
    }
    printf("Socket bound to address successfully\n");

    if (listen(sockfd, SOMAXCONN) != 0) {
        perror("webserver (listen)");
        return 1;
    }
    printf("Server is listening for connections\n");


    for (;;) {

        int* newsockfd = malloc(sizeof(int));
        *newsockfd = accept(sockfd, (struct sockaddr*)&host_addr, (socklen_t*)&host_addrlen);
        if (newsockfd < 0) {
            perror("webserver (accept)");
            free(newsockfd);
            continue;
        }

        printf("Connection accepted\n");

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, newsockfd) != 0) {
            perror("pthread_create");
            close(*newsockfd);
            free(newsockfd);
        }
        else {
            pthread_detach(tid);
        }
    }
    close(sockfd);
    return 0;
}