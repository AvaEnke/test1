#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

// Function to handle receiving messages from the server
void *receive_messages(void *sock_fd) {
    int sockfd = *(int *)sock_fd;
    char buffer[1024];
    int len;

    while ((len = recv(sockfd, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[len] = '\0'; // Null-terminate the received data
        printf("Received: %s\n", buffer);
    }

    if (len == 0) {
        printf("Server closed the connection.\n");
    } else if (len == -1) {
        perror("recv error");
    }

    close(sockfd);
    exit(1);
}

// Function to handle sending messages to the server
void *send_messages(void *sock_fd) {
    int sockfd = *(int *)sock_fd;
    char buffer[1024];

    printf("Enter messages: \n");
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Remove newline character at the end if present
        int len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        strcat(buffer, "\n"); // Ensure that each message ends with a newline

        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            perror("send failed");
            break;
        }
    }

    close(sockfd);
    exit(1);
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP_address> <port_number>\n", argv[0]);
        exit(1);
    }

    int sockfd;
    struct sockaddr_in server_addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Could not create socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));  // Convert string port number to integer and use network byte order
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);  // Convert IP address from string to network byte order

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        exit(1);
    }

    printf("Connected to the server. You can start sending messages.\n");

    pthread_t recv_thread, send_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sockfd);
    pthread_create(&send_thread, NULL, send_messages, &sockfd);

    // Wait for threads to finish
    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    close(sockfd);
    return 0;
}

