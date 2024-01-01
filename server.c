#include "server.h"

void handle_request(int client_socket) {
    char request[MAX_REQUEST_SIZE];
    ssize_t bytes_received = recv(client_socket, request, sizeof(request) - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to read request");
        close(client_socket);
        return;
    }

    request[bytes_received] = '\0';
    printf("\n%s\n", request);
    if (strncmp(request, "GET /HEADER", 11) == 0) {
        // heandle request for header
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
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char entry_path[2048];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", file_path, entry->d_name);

            struct stat entry_stat;
            if (stat(entry_path, &entry_stat) == -1) {
                perror("Failed to get file status");
                continue;
            }

            char entry_link[1024];
            if (S_ISDIR(entry_stat.st_mode)) {
                snprintf(entry_link, sizeof(entry_link),
                        "<li><a href=\"/HTTP/%s\">%s/</a></li>", entry->d_name, entry->d_name);
            } else {
                snprintf(entry_link, sizeof(entry_link),
                        "<li><a href=\"/HTTP/%s\">%s</a></li>", entry->d_name, entry->d_name);
            }

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