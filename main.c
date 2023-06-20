#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_REQUEST_SIZE 1024
#define RESPONSE_BUFFER_SIZE 4096
#define DOCUMENT_ROOT "/home/"

void request(int client_socket) {
    char request[MAX_REQUEST_SIZE];
    ssize_t bytes_received = recv(client_socket, request, sizeof(request) - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to read request");
        return;
    }

    // Add null terminator to the received data
    request[bytes_received] = '\0';
    printf("\n %s \n", request);
    // Check if the requested path is the special path "/HEADER"
    if (strcmp(request, "GET /HEADER") == 0) {
        // Return the HTTP request header as HTML
        const char *header_response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                      "<html><body><h1>HTTP Request Header</h1>"
                                      "<pre>%s</pre></body></html>";
        char response_buffer[RESPONSE_BUFFER_SIZE];
        snprintf(response_buffer, sizeof(response_buffer), header_response, request);
        send(client_socket, response_buffer, strlen(response_buffer), 0);
        close(client_socket);
        return;
    }

    // Extract the requested file path from the request
    char *path = strtok(request, " ");
    path = strtok(NULL, " ");

    // Concatenate the document root and requested file path
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s%s", DOCUMENT_ROOT, path);

    // Check if the requested path is a directory
    struct stat file_stat;
    if (stat(file_path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
        // The requested path is a directory, generate a directory listing

        // Open the directory
        DIR *dir = opendir(file_path);
        if (dir == NULL) {
            perror("Failed to open directory");
            const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n"
                                         "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                         "<html><body><h1>500 Internal Server Error</h1></body></html>";
            send(client_socket, error_response, strlen(error_response), 0);
            close(client_socket);
            return;
        }

        // Prepare the directory listing response
        char response_buffer[RESPONSE_BUFFER_SIZE];
        snprintf(response_buffer, sizeof(response_buffer),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                 "<html><body><h1>Directory Listing</h1><ul>");

        // Read each entry in the directory
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Ignore the current and parent directory entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            // Add the entry as a link in the directory listing response
            char entry_link[512];
            snprintf(entry_link, sizeof(entry_link),
                     "<li><a href=\"%s/%s\">%s</a></li>", path, entry->d_name, entry->d_name);
            strncat(response_buffer, entry_link, sizeof(response_buffer) - strlen(response_buffer) - 1);
        }

        // Close the directory
        closedir(dir);

        // Complete the directory listing response
        strncat(response_buffer, "</ul></body></html>", sizeof(response_buffer) - strlen(response_buffer) - 1);

        // Send the directory listing response
        send(client_socket, response_buffer, strlen(response_buffer), 0);
    } else {
        // The requested path is a file, attempt to open and send it as a response

        // Open the file in read-only mode
        int file_fd = open(file_path, O_RDONLY);
        if (file_fd == -1) {
            // File not found or failed to open
            perror("Failed to open file");
            const char *not_found_response = "HTTP/1.1 404 Not Found\r\n"
                                             "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                             "<html><body><h1>404 Not Found</h1></body></html>";
            send(client_socket, not_found_response, strlen(not_found_response), 0);
        } else {
            // File found, read its contents and send as response
            struct stat file_stat;
            fstat(file_fd, &file_stat);
            off_t file_size = file_stat.st_size;

            char response_header[256];
            snprintf(response_header, sizeof(response_header),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/octet-stream\r\n"
                     "Content-Disposition: attachment; filename=\"%s\"\r\n"
                     "Content-Length: %ld\r\n\r\n",
                     path, file_size);
            send(client_socket, response_header, strlen(response_header), 0);

            char buffer[RESPONSE_BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
                send(client_socket, buffer, bytes_read, 0);
            }

            close(file_fd);
        }
    }

    // Close the client socket
    close(client_socket);
}

int main(int argc, char *argv[]){
    
    if(argc < 2){
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    
    int port = atoi(argv[1]);
    int server_socket;
    struct sockaddr_in server_addr;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Prepare the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind the socket to the specified address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_socket, 10) == -1) {
        perror("Failed to listen for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // Accept a client connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Failed to accept connection");
            continue;
        }

        // Handle the client request
        request(client_socket);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
