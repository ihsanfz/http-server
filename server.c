#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h> 
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_LOC "./files"

const char* get_mime_type(const char* path) {

    char* ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcasecmp(ext, ".html") == 0) {
        return "text/html";
    }
    else if (strcasecmp(ext, ".css") == 0) {
        return "text/css";
    }
    else if (strcasecmp(ext, ".jpg") == 0) {
        return "image/jpeg";
    }
    else if (strcasecmp(ext, ".png") == 0) {
        return "image/png";
    }
    else if (strcasecmp(ext, ".js") == 0) {
        return "application/javascript";
    }
    else {
        return "application/octet-stream";
    }
}

void* handle_client(void* arg) {
    int newsockfd = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    int sockn = getpeername(newsockfd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addrlen);

    int valread = read(newsockfd, buffer, BUFFER_SIZE);

    buffer[valread] = "\0";
    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);
    printf("[%s:%u] %s %s %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), method, version, uri);

    char path[BUFFER_SIZE];
    if (strcmp(uri, "/") == 0) {
        snprintf(path, sizeof(path), "%s/index.html", FILE_LOC);
    }
    else {
        snprintf(path, sizeof(path), "%s%s", FILE_LOC, uri);
    }

    FILE* file = fopen(path, "rb");
    if (file) {
        struct stat st;
        stat(path, &st);
        size_t filesize = st.st_size;

        char header[BUFFER_SIZE];
        snprintf(header, sizeof(header),
            "HTTP/1.0 200 OK\r\n"
            "Server: webserver-c\r\n"
            "Content-Length: %zu\r\n"
            "Content-Type: %s\r\n\r\n",
            filesize, get_mime_type(path));
        write(newsockfd, header, strlen(header));

        char filebuf[BUFFER_SIZE];
        size_t n;
        while ((n = fread(filebuf, 1, sizeof(filebuf), file)) > 0) {
            write(newsockfd, filebuf, n);
        }
        fclose(file);
    } else {
        char* not_found = "HTTP/1.0 404 Not Found\r\n"
            "Server: webserver-c\r\n"
            "Content-type: text/html\r\n\r\n"
            "<html>File not found</html>\r\n";
        write(newsockfd, not_found, strlen(not_found));
    }
    close(newsockfd);
    return NULL;
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