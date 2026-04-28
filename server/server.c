#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "auth.h"
#include "chat.h"
#include "utils.h"
#include "network_server.h"
#include "../shared/models.h"
#include "../shared/network_config.h"

#define USERS_FILE "../data/users.txt"
#define MESSAGES_FILE "../data/messages.txt"

// Global server socket
int server_socket;
volatile int server_running = 1;

// Sends a response back to a specific UDP client
void send_response(char response[], struct sockaddr_in *client_addr, socklen_t addr_len) {
    char full_response[BUFFER_SIZE];
    snprintf(full_response, BUFFER_SIZE, "%s\n", response);
    int response_len = strlen(full_response);
    
    int sent = sendto(server_socket, full_response, response_len, 0,
                      (struct sockaddr *)client_addr, addr_len);
    if (sent == -1) {
        printf("[!] Error sending response: %s\n", strerror(errno));
    }
}

// Processes a single UDP request (runs in a child process)
void process_request(char buffer[], struct sockaddr_in *client_addr, socklen_t addr_len) {
    char fields[10][256];
    int field_count;
    
    // Parse the request
    parse_request(buffer, fields, &field_count);
    
    if (field_count == 0) {
        send_response("ERROR|Empty request", client_addr, addr_len);
        return;
    }
    
    // Determine request type and delegate to appropriate module
    char *request_type = fields[0];
    
    // Log request with PID to demonstrate concurrency
    if (field_count >= 2 && strcmp(request_type, "FETCH") != 0 && strcmp(request_type, "CONTACTS") != 0) {
        printf("[Child PID: %d] Processing [%s] from %s\n", getpid(), request_type, fields[1]);
    }
    
    if (strcmp(request_type, "REGISTER") == 0) {
        if (field_count >= 3) {
            register_user(fields[1], fields[2], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid REGISTER request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "LOGIN") == 0) {
        if (field_count >= 3) {
            authenticate_user(fields[1], fields[2], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid LOGIN request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "SEARCH") == 0) {
        if (field_count >= 3) {
            search_user(fields[1], fields[2], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid SEARCH request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "DEREGISTER") == 0) {
        if (field_count >= 2) {
            deregister_user(fields[1], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid DEREGISTER request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "LOGOUT") == 0) {
        if (field_count >= 2) {
            logout_user(fields[1], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid LOGOUT request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "SEND") == 0) {
        if (field_count >= 4) {
            send_message(fields[1], fields[2], fields[3], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid SEND request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "FETCH") == 0) {
        if (field_count >= 3) {
            fetch_messages(fields[1], fields[2], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid FETCH request format", client_addr, addr_len);
        }
    }
    else if (strcmp(request_type, "CONTACTS") == 0) {
        if (field_count >= 2) {
            fetch_contacts(fields[1], client_addr, addr_len);
        } else {
            send_response("ERROR|Invalid CONTACTS request format", client_addr, addr_len);
        }
    }
    else {
        send_response("ERROR|Unknown request type", client_addr, addr_len);
    }
}

// Signal handler for SIGCHLD to prevent zombie processes
void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Signal handler for clean shutdown
void shutdown_server(int sig) {
    (void)sig;
    printf("\n[+] Server shutting down. Goodbye.\n");
    server_running = 0;
    close(server_socket);
    cleanup_server_network();
    destroy_data_mutex();
    exit(0);
}

int main() {
    struct sockaddr_in server_addr;
    int discovery_socket;
    fd_set read_fds;
    
    // Set up signal handlers
    signal(SIGINT, shutdown_server);
    signal(SIGCHLD, handle_sigchld); // Reap child processes to prevent zombies
    
    // Initialize the file lock for process-safe data access
    init_data_mutex();
    
    // Create data files if they don't exist
    create_file_if_missing(USERS_FILE);
    create_file_if_missing(MESSAGES_FILE);
    
    if (!initialize_server_network()) {
        return 1;
    }

    discovery_socket = get_discovery_socket();

    // Create UDP socket
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1) {
        printf("[!] Socket creation failed\n");
        return 1;
    }
    
    // Set socket options to allow address reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        printf("[!] Bind failed\n");
        close(server_socket);
        return 1;
    }

    printf("[+] Concurrent server started (fork). Listening on port %d...\n\n", PORT);
    
    // Main server loop - fork a child process for each incoming request
    while (server_running) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        int max_fd = server_socket;

        if (discovery_socket != -1) {
            FD_SET(discovery_socket, &read_fds);
            if (discovery_socket > max_fd) {
                max_fd = discovery_socket;
            }
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by SIGCHLD, just retry
            }
            if (server_running) {
                printf("[!] Select failed\n");
            }
            continue;
        }

        if (discovery_socket != -1 && FD_ISSET(discovery_socket, &read_fds)) {
            handle_discovery_request(discovery_socket);
        }

        if (FD_ISSET(server_socket, &read_fds)) {
            char buffer[BUFFER_SIZE];
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            // Receive the datagram in the parent process
            int bytes = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0,
                                 (struct sockaddr *)&client_addr, &addr_len);
            if (bytes <= 0) {
                printf("[!] Error receiving data\n");
                continue;
            }
            buffer[bytes] = '\0';

            // Parse request type for logging (excluding spammy polling endpoints)
            char req_type[32] = {0};
            sscanf(buffer, "%31[^|]", req_type);
            int should_log = (strcmp(req_type, "FETCH") != 0 && strcmp(req_type, "CONTACTS") != 0 && strlen(req_type) > 0);

            if (should_log) {
                printf("[Parent PID: %d] Received %s request, forking child...\n", getpid(), req_type);
            }

            // Fork a child process to handle this request
            pid_t pid = fork();
            if (pid > 0) {
                // Parent: continues the loop immediately
                if (should_log) {
                    printf("[Parent PID: %d] Delegated %s request to Child PID: %d\n", getpid(), req_type, pid);
                }
            } else if (pid == 0) {
                // Child process: handle the request, then exit
                process_request(buffer, &client_addr, addr_len);
                fflush(stdout);
                close(server_socket);
                _exit(0);
            } else if (pid < 0) {
                printf("[!] Fork failed\n");
            }
        }
    }
    
    // Cleanup
    close(server_socket);
    cleanup_server_network();
    destroy_data_mutex();
    
    return 0;
}
