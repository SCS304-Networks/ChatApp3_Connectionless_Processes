#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>

#include "ui.h"
#include "network_client.h"
#include "../shared/models.h"

// Global state
int is_logged_in = 0;
int app_running = 1;
char session_user[MAX_USERNAME];

// Function prototypes
int connect_to_server();
void send_request(int sock, char request[]);
void receive_response(int sock, char buffer[]);
void sanitize_input(char buffer[]);
void display_main_menu();
void display_dashboard();
void handle_register();
void handle_login();
void handle_search();
void handle_deregister();
void handle_logout();
void handle_inbox();
void handle_chat(char partner[]);
void send_chat_message(char partner[], char text[]);
int fetch_and_display_messages(char partner[], int should_redraw);
int input_available();
void set_terminal_raw_mode(int enable);

// Strips unwanted characters from input
void sanitize_input(char buffer[]) {
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    
    char *cr = strchr(buffer, '\r');
    if (cr) *cr = '\0';
    
    // Remove pipe characters
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '|') {
            buffer[i] = ' ';
        }
    }
}

// Creates a UDP socket and sets the server as the default destination
int connect_to_server() {
    int sock;
    struct sockaddr_in server_addr;
    
    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        ui_server_error();
        return -1;
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(get_server_ip());
    
    // Associate the server address with this socket
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        ui_server_error();
        close(sock);
        return -1;
    }


    // Set a 5-second receive timeout to prevent hanging if server doesn't respond
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return sock;
}

// Sends a request to the server
void send_request(int sock, char request[]) {
    // Send standard UDP datagram
    send(sock, request, strlen(request), 0);
}

// Receives a response from the server
void receive_response(int sock, char buffer[]) {
    int received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (received > 0) {
        buffer[received] = '\0';
        char *nl = strchr(buffer, '\n');
        if (nl) *nl = '\0';
    } else {
        buffer[0] = '\0';
    }
}

// Checks if keyboard input is available (non-blocking)
int input_available() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

// Handles the registration flow
void handle_register() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    ui_display_register_screen();
    
    printf("  Enter desired username: ");
    fgets(username, MAX_USERNAME, stdin);
    sanitize_input(username);
    
    printf("  Enter desired password: ");
    fgets(password, MAX_PASSWORD, stdin);
    sanitize_input(password);
    
    // Connect and send request
    int sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    printf("[Network] Successfully connected to the server IP address %s\n", get_server_ip());
    
    snprintf(request, BUFFER_SIZE, "REGISTER|%s|%s\n", username, password);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
    
    // Parse response
    if (strncmp(response, "SUCCESS", 7) == 0) {
        char *msg = strchr(response, '|');
        ui_success(msg ? msg + 1 : "Account created");
    } else {
        char *msg = strchr(response, '|');
        ui_error(msg ? msg + 1 : "Registration failed");
    }
    
    ui_wait_for_enter();
}

// Handles the login flow
void handle_login() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    ui_display_login_screen();
    
    printf("  Enter username: ");
    fgets(username, MAX_USERNAME, stdin);
    sanitize_input(username);
    
    printf("  Enter password: ");
    fgets(password, MAX_PASSWORD, stdin);
    sanitize_input(password);
    
    // Connect and send request
    int sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    printf("[Network] Successfully connected to the server IP address %s\n", get_server_ip());
    
    snprintf(request, BUFFER_SIZE, "LOGIN|%s|%s\n", username, password);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
    
    // Parse response
    if (strncmp(response, "SUCCESS", 7) == 0) {
        char *msg = strchr(response, '|');
        ui_success(msg ? msg + 1 : "Login successful");
        strncpy(session_user, username, MAX_USERNAME - 1);
        session_user[MAX_USERNAME - 1] = '\0';
        is_logged_in = 1;
    } else {
        char *msg = strchr(response, '|');
        ui_error(msg ? msg + 1 : "Login failed");
    }
    
    ui_wait_for_enter();
}

// Handles the user search flow
void handle_search() {
    char target[MAX_USERNAME];
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    ui_display_search_screen();
    
    printf("  Enter username to search: ");
    fgets(target, MAX_USERNAME, stdin);
    sanitize_input(target);
    
    // Connect and send request
    int sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    
    snprintf(request, BUFFER_SIZE, "SEARCH|%s|%s\n", session_user, target);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
    
    // Parse response
    if (strncmp(response, "SUCCESS", 7) == 0) {
        char *msg = strchr(response, '|');
        ui_success(msg ? msg + 1 : "User found");
    } else {
        char *msg = strchr(response, '|');
        ui_error(msg ? msg + 1 : "User not found");
    }
    
    ui_wait_for_enter();
}

// Handles the deregistration flow
void handle_deregister() {
    char confirm;
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    ui_display_deregister_warning();
    confirm = getchar();
    
    // Clear input buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    
    if (confirm != 'y' && confirm != 'Y') {
        return;
    }
    
    // Connect and send request
    int sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    
    snprintf(request, BUFFER_SIZE, "DEREGISTER|%s\n", session_user);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
    
    // Parse response
    if (strncmp(response, "SUCCESS", 7) == 0) {
        char *msg = strchr(response, '|');
        ui_success(msg ? msg + 1 : "Account deleted");
        memset(session_user, 0, sizeof(session_user));
        is_logged_in = 0;
    } else {
        char *msg = strchr(response, '|');
        ui_error(msg ? msg + 1 : "Deregistration failed");
    }
    
    ui_wait_for_enter();
}

// Handles the logout flow
void handle_logout() {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    // Connect and send request
    int sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    
    snprintf(request, BUFFER_SIZE, "LOGOUT|%s\n", session_user);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
    
    // Parse response and logout regardless
    char *msg = strchr(response, '|');
    ui_success(msg ? msg + 1 : "Logged out");
    
    memset(session_user, 0, sizeof(session_user));
    is_logged_in = 0;
    
    ui_wait_for_enter();
}

// Handles the inbox/chat partner selection
void handle_inbox() {
    char partner[MAX_USERNAME];
    char input[MAX_USERNAME];
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char contacts[50][50];
    int contact_count = 0;
    
    // Fetch contacts list
    int sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    
    snprintf(request, BUFFER_SIZE, "CONTACTS|%s\n", session_user);
    send_request(sock, request);
    
    // Receive contacts until END
    while (contact_count < 50) {
        receive_response(sock, response);
        
        if (strcmp(response, "END") == 0 || strlen(response) == 0) {
            break;
        }
        
        if (strncmp(response, "CONTACT|", 8) == 0) {
            strncpy(contacts[contact_count], response + 8, 49);
            contacts[contact_count][49] = '\0';
            contact_count++;
        }
    }
    close(sock);
    
    // Display inbox with contacts
    ui_display_inbox(contacts, contact_count);
    
    printf("  > ");
    fgets(input, MAX_USERNAME, stdin);
    sanitize_input(input);
    
    // Check if input is a number (contact selection)
    int selection = atoi(input);
    if (selection > 0 && selection <= contact_count) {
        strncpy(partner, contacts[selection - 1], MAX_USERNAME - 1);
        partner[MAX_USERNAME - 1] = '\0';
    } else {
        strncpy(partner, input, MAX_USERNAME - 1);
        partner[MAX_USERNAME - 1] = '\0';
    }
    
    if (strlen(partner) == 0) {
        return;
    }
    
    // Validate partner exists
    sock = connect_to_server();
    if (sock == -1) {
        ui_wait_for_enter();
        return;
    }
    
    snprintf(request, BUFFER_SIZE, "SEARCH|%s|%s\n", session_user, partner);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
    
    if (strncmp(response, "ERROR", 5) == 0) {
        char *msg = strchr(response, '|');
        ui_error(msg ? msg + 1 : "User not found");
        ui_wait_for_enter();
        return;
    }
    
    // Launch chat with validated partner
    handle_chat(partner);
}

// Enables or disables terminal raw mode for non-blocking character input
void set_terminal_raw_mode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        // Disable canonical mode and echo
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        // Restore previous settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// Fetches messages for a conversation. Returns the total count of messages fetched.
// If should_redraw is 1, it will print the messages to the screen.
int fetch_and_display_messages(char partner[], int should_redraw) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    int msg_count = 0;
    
    int sock = connect_to_server();
    if (sock == -1) {
        return 0;
    }
    
    snprintf(request, BUFFER_SIZE, "FETCH|%s|%s\n", session_user, partner);
    send_request(sock, request);
    
    // Display chat header if redrawing
    if (should_redraw) {
        ui_display_chat_screen(session_user, partner);
    }
    
    // Receive and display messages until END
    while (1) {
        receive_response(sock, response);
        
        if (strcmp(response, "END") == 0 || strlen(response) == 0) {
            break;
        }
        
        if (strncmp(response, "DATA", 4) == 0) {
            msg_count++;
            
            if (should_redraw) {
                // Parse: DATA|id|sender|receiver|text|timestamp
                char *fields[6];
                char temp[BUFFER_SIZE];
                strncpy(temp, response, BUFFER_SIZE - 1);
                temp[BUFFER_SIZE - 1] = '\0';
                
                int field_count = 0;
                char *token = strtok(temp, "|");
                while (token && field_count < 6) {
                    fields[field_count++] = token;
                    token = strtok(NULL, "|");
                }
                
                if (field_count >= 6) {
                    ui_display_message(fields[2], fields[5], fields[4]);
                }
            }
        }
    }
    
    close(sock);
    return msg_count;
}

// Sends a chat message
void send_chat_message(char partner[], char text[]) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    sanitize_input(text);
    
    if (strlen(text) == 0) {
        return;
    }
    
    int sock = connect_to_server();
    if (sock == -1) {
        return;
    }
    
    snprintf(request, BUFFER_SIZE, "SEND|%s|%s|%s\n", session_user, partner, text);
    send_request(sock, request);
    receive_response(sock, response);
    close(sock);
}

// Handles the active chat session
void handle_chat(char partner[]) {
    char input[MAX_MESSAGE] = {0};
    int input_len = 0;
    int last_message_count = 0;
    time_t last_fetch_time = time(NULL);
    
    // Initial message fetch and display
    last_message_count = fetch_and_display_messages(partner, 1);
    printf("\n  [Auto-refresh active] Type /q to quit.\n");
    printf("\n  You: ");
    fflush(stdout);
    
    // Disable canonical mode so we can capture characters instantly
    set_terminal_raw_mode(1);
    
    while (1) {
        struct timeval tv = {1, 0}; // 1 second timeout
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        int res = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        
        if (res > 0) {
            // User typed something
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == '\n' || c == '\r') {
                    if (strcmp(input, "/q") == 0) {
                        break;
                    }
                    if (input_len > 0) {
                        send_chat_message(partner, input);
                        input[0] = '\0';
                        input_len = 0;
                        
                        // Immediately fetch and redraw after sending
                        last_message_count = fetch_and_display_messages(partner, 1);
                        printf("\n  [Auto-refresh active] Type /q to quit.\n");
                        printf("\n  You: ");
                        fflush(stdout);
                        last_fetch_time = time(NULL);
                    }
                } else if (c == 127 || c == '\b') {
                    // Handle backspace properly
                    if (input_len > 0) {
                        input_len--;
                        input[input_len] = '\0';
                        printf("\b \b"); // Erase character visually from terminal
                        fflush(stdout);
                    }
                } else if (c >= 32 && c <= 126 && input_len < MAX_MESSAGE - 1) {
                    // Printable character
                    input[input_len++] = c;
                    input[input_len] = '\0';
                    putchar(c);
                    fflush(stdout);
                }
            }
        } 
        
        // Auto-refresh mechanism (poll every 2 seconds)
        if (time(NULL) - last_fetch_time >= 2) {
            last_fetch_time = time(NULL);
            
            // Quietly fetch the message count
            int current_count = fetch_and_display_messages(partner, 0);
            
            if (current_count > last_message_count) {
                // New messages arrived! Redraw the screen seamlessly
                last_message_count = fetch_and_display_messages(partner, 1);
                printf("\n  [Auto-refresh active] Type /q to quit.\n");
                
                // Restore the user's currently typed input
                printf("\n  You: %s", input);
                fflush(stdout);
            }
        }
    }
    
    // Ensure we restore the terminal mode before returning
    set_terminal_raw_mode(0);
}

// Displays the main menu and handles selection
void display_main_menu() {
    int choice;
    
    ui_display_main_menu();
    
    if (scanf("%d", &choice) != 1) {
        // Clear invalid input
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return;
    }
    // Clear the newline
    while (getchar() != '\n');
    
    switch (choice) {
        case 1:
            handle_login();
            break;
        case 2:
            handle_register();
            break;
        case 3:
            app_running = 0;
            break;
        default:
            break;
    }
}

// Displays the dashboard and handles selection
void display_dashboard() {
    int choice;
    
    ui_display_dashboard(session_user);
    
    if (scanf("%d", &choice) != 1) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return;
    }
    while (getchar() != '\n');
    
    switch (choice) {
        case 1:
            handle_inbox();
            break;
        case 2:
            handle_search();
            break;
        case 3:
            handle_deregister();
            break;
        case 4:
            handle_logout();
            break;
        default:
            break;
    }
}

int main() {
    int mode = 0;
    while (mode != 1 && mode != 2 && mode != 3) {
        printf("\nSelect connection mode:\n[1] Tailnet\n[2] Same LAN\n[3] Manual IP\nChoice: ");
        char input[16];
        if (!fgets(input, sizeof(input), stdin)) {
            continue;
        }
        mode = atoi(input);
        if (mode != 1 && mode != 2 && mode != 3) {
            printf("Invalid choice. Please enter 1, 2, or 3.\n");
        }
    }

    if (!initialize_server_address(mode)) {
        printf("Failed to determine server address.\n");
        return 1;
    }

    // Main application loop
    while (app_running) {
        if (is_logged_in) {
            display_dashboard();
        } else {
            display_main_menu();
        }
    }

    ui_clear_screen();
    printf("\n  Goodbye!\n\n");

    return 0;
}
