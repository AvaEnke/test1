#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct client_node {
    int socket_fd;
    char name[100];
    struct client_node *next;
} client_node;

client_node *head = NULL;
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

int unique_id = 0;  // Used to assign unique names to users

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void broadcast_message(char *message, int sender_fd);

void add_client(client_node *new_client) {
    pthread_mutex_lock(&client_list_mutex);
    sprintf(new_client->name, "User%d", unique_id++);  // Assign a unique name to each client
    new_client->next = head;
    head = new_client;
    char message[128];
    sprintf(message, "%s has connected.\n", new_client->name);
    broadcast_message(message, -1);  // Broadcast the connection message
    pthread_mutex_unlock(&client_list_mutex);
}

void remove_client(int socket_fd) {
    pthread_mutex_lock(&client_list_mutex);
    client_node *temp = head, *prev = NULL;
    char message[128];
    
    while (temp != NULL && temp->socket_fd != socket_fd) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return;

    sprintf(message, "%s has quit.\n", temp->name);

    if (prev == NULL) {
        head = temp->next;
    } else {
        prev->next = temp->next;
    }

    broadcast_message(message, -1);  // Notify others of the disconnection
    free(temp);
    pthread_mutex_unlock(&client_list_mutex);
}

void broadcast_message(char *message, int sender_fd) {
    pthread_mutex_lock(&client_list_mutex);
    client_node *temp = head;
    while (temp != NULL) {
        if (temp->socket_fd != sender_fd) {
            send(temp->socket_fd, message, strlen(message), 0);
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&client_list_mutex);
}

void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE], message[BUFFER_SIZE + 100];  // Ensure buffer is large enough
    int read_size;

    client_node *current_client = head;
    while(current_client && current_client->socket_fd != sock)
        current_client = current_client->next;

    if (!current_client) return 0;

    // Receive messages from client
    while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0'; // Ensure null termination

        // Handling commands
        if (strncmp(buffer, "name ", 5) == 0) {
            char old_name[100];
            strcpy(old_name, current_client->name);
            sscanf(buffer, "name %[^\n]", current_client->name);
            snprintf(message, sizeof(message), "%s has changed their name to %s.\n", old_name, current_client->name);
            broadcast_message(message, -1);
        } else if (strcmp(buffer, "quit\n") == 0) {
            break;
        } else {
            snprintf(message, sizeof(message), "%s: %s", current_client->name, buffer);
            broadcast_message(message, sock);
        }
    }

    if (read_size == 0) {
        printf("Client %s disconnected\n", current_client->name);
    } else if (read_size == -1) {
        perror("recv failed");
    }

    remove_client(sock);
    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    int server_fd, *new_sock;
    struct sockaddr_in server, client;
    socklen_t client_len;
    pthread_t thread_id;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);//replace 
    if (server_fd == -1) {
        error("Could not create socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));

    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        error("bind failed");
    }

    listen(server_fd, MAX_CLIENTS);

    puts("Waiting for incoming connections...");
    client_len = sizeof(struct sockaddr_in);

    while (1) {
        new_sock = malloc(sizeof(int));
        *new_sock = accept(server_fd, (struct sockaddr *)&client, &client_len);
        if (*new_sock >= 0) {
            client_node *new_client = malloc(sizeof(client_node));
            new_client->socket_fd = *new_sock;
            new_client->next = NULL;
            add_client(new_client);
            if (pthread_create(&thread_id, NULL, client_handler, (void*) new_sock) < 0) {
                error("could not create thread");
            }
            printf("Handler assigned for socket %d\n", *new_sock);
        } else {
            free(new_sock);
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                error("accept failed");
            }
        }
    }

    close(server_fd);
    pthread_mutex_destroy(&client_list_mutex);
    return 0;
}

