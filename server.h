#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_REQUEST_SIZE 1024
#define RESPONSE_BUFFER_SIZE 4096
#define DOCUMENT_ROOT "/home/USER" // Altere para o diretorio especifico

void handle_request(int client_socket);