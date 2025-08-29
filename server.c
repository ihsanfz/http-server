#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h> 
#include <sys/stat.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define PATH_SIZE 4096
#define FILE_LOC "./files"

const char* not_found =
"HTTP/1.0 404 Not Found\r\n"
"Content-Type: text/html\r\n\r\n"
"<h1>404 Not Found</h1>";

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

    buffer[valread] = '\0';
    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);

    if (strstr(uri, "..")) {
        const char* bad = "HTTP/1.0 400 Bad Request\r\n"
            "Content - Type: text / html\r\n\r\n"
            "<h1>400 Bad Request</h1>";
        write(newsockfd, bad, strlen(bad));
        close(newsockfd);
        return NULL;
    }

    printf("[%s:%u] %s %s %s\n",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port),
        method, version, uri);

    char base_path[PATH_SIZE];
    if (strcmp(uri, "/") == 0) {
        snprintf(base_path, sizeof(base_path), "%s", FILE_LOC);
    }
    else {
        snprintf(base_path, sizeof(base_path), "%s%s", FILE_LOC, uri);
    }

    struct stat st;
    if (stat(base_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            char index_path[PATH_SIZE];
            snprintf(index_path, sizeof(index_path), "%s/index.html", base_path);

            FILE* index = fopen(index_path, "rb");
            if (index) {
                fseek(index, 0, SEEK_END);
                size_t filesize = ftell(index);
                rewind(index);

                char header[BUFFER_SIZE];
                snprintf(header, sizeof(header),
                    "HTTP/1.0 200 OK\r\n"
                    "Server: webserver-c\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: text/html\r\n\r\n",
                    filesize);
                write(newsockfd, header, strlen(header));

                char filebuf[BUFFER_SIZE];
                size_t n;
                while ((n = fread(filebuf, 1, sizeof(filebuf), index)) > 0) {
                    write(newsockfd, filebuf, n);
                }
                fclose(index);
            }
            else {
                DIR* dir = opendir(base_path);
                if (!dir) {
                    const char* forbidden =
                        "HTTP/1.0 403 Forbidden\r\n"
                        "Server: webserver-c\r\n"
                        "Content-Type: text/html\r\n\r\n"
                        "<h1>403 Forbidden</h1>";
                    write(newsockfd, forbidden, strlen(forbidden));
                }
                else {
                    const char* hdr =
                        "HTTP/1.0 200 OK\r\n"
                        "Server: webserver-c\r\n"
                        "Content-Type: text/html\r\n\r\n";
                    write(newsockfd, hdr, strlen(hdr));

                    const char* head = "<html><body><h1>Directory listing</h1><ul>";
                    write(newsockfd, head, strlen(head));

                    if (strcmp(uri, "/") != 0) {
                        char parent[PATH_SIZE];
                        strncpy(parent, uri, sizeof(parent));
                        parent[sizeof(parent) - 1] = '\0';
                        size_t len = strlen(parent);
                        if (len > 1 && parent[len - 1] == '/') parent[len - 1] = '\0';
                        char* slash = strrchr(parent, '/');
                        if (slash && slash != parent) *(slash + 1) = '\0';
                        else strcpy(parent, "/");

                        char up[PATH_SIZE];
                        snprintf(up, sizeof(up), "<li><a href=\"%s\">..</a></li>", parent);
                        write(newsockfd, up, strlen(up));
                    }

                    struct dirent* entry;
                    while ((entry = readdir(dir)) != NULL) {
                        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                            continue;

                        char full_uri[PATH_SIZE];
                        if (uri[strlen(uri) - 1] == '/')
                            snprintf(full_uri, sizeof(full_uri), "%s%s", uri, entry->d_name);
                        else
                            snprintf(full_uri, sizeof(full_uri), "%s/%s", uri, entry->d_name);

                        char line[PATH_SIZE];
                        snprintf(line, sizeof(line),
                            "<li><a href=\"%s\">%s</a></li>",
                            full_uri, entry->d_name);
                        write(newsockfd, line, strlen(line));
                    }
                    closedir(dir);

                    const char* tail = "</ul></body></html>";
                    write(newsockfd, tail, strlen(tail));
                }
            }
        }
        else if (S_ISREG(st.st_mode)) {
            FILE* file = fopen(base_path, "rb");
            if (file) {
                size_t filesize = st.st_size;

                char header[BUFFER_SIZE];
                snprintf(header, sizeof(header),
                    "HTTP/1.0 200 OK\r\n"
                    "Server: webserver-c\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: %s\r\n\r\n",
                    filesize, get_mime_type(base_path));
                write(newsockfd, header, strlen(header));

                char filebuf[BUFFER_SIZE];
                size_t n;
                while ((n = fread(filebuf, 1, sizeof(filebuf), file)) > 0) {
                    write(newsockfd, filebuf, n);
                }
                fclose(file);
            }
            else {
                const char* err =
                    "HTTP/1.0 500 Internal Server Error\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<h1>500 Internal Server Error</h1>";
                write(newsockfd, err, strlen(err));
            }
        }
        else {
            write(newsockfd, not_found, strlen(not_found));
        }
    }
    else {
        write(newsockfd, not_found, strlen(not_found));
    }
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
        *newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addrlen);
        if (*newsockfd < 0) {
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