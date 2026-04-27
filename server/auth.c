#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "auth.h"
#include "utils.h"
#include "../shared/models.h"

#define USERS_FILE "../data/users.txt"

// Forward declaration
void send_response(char response[], struct sockaddr_in *client_addr, socklen_t addr_len);

// Checks if a username already exists in the registry
// NOTE: Caller must hold the data file lock
int validate_unique_user(char username[]) {
    FILE *file = fopen(USERS_FILE, "r");
    if (file == NULL) {
        return 1; // File doesn't exist, username is unique
    }
    
    char line[256];
    char stored_username[MAX_USERNAME];
    
    while (fgets(line, sizeof(line), file)) {
        // Parse username from line (format: username|password)
        char *delimiter = strchr(line, '|');
        if (delimiter) {
            int len = delimiter - line;
            strncpy(stored_username, line, len);
            stored_username[len] = '\0';
            
            if (strcmp(stored_username, username) == 0) {
                fclose(file);
                return 0; // Username exists, not unique
            }
        }
    }
    
    fclose(file);
    return 1; // Username is unique
}

// Processes a REGISTER request by creating a new user account
void register_user(char username[], char password[],
                   struct sockaddr_in *client_addr, socklen_t addr_len) {
    char response[BUFFER_SIZE];
    
    // Sanitize inputs
    sanitize_input(username);
    sanitize_input(password);
    
    // Check for empty inputs
    if (strlen(username) == 0 || strlen(password) == 0) {
        snprintf(response, BUFFER_SIZE, "ERROR|Username and password cannot be empty");
        send_response(response, client_addr, addr_len);
        return;
    }
    
    // Lock before file access
    lock_data_files();
    
    // Check if username already exists
    if (!validate_unique_user(username)) {
        unlock_data_files();
        snprintf(response, BUFFER_SIZE, "ERROR|Username already taken");
        send_response(response, client_addr, addr_len);
        return;
    }
    
    // Append new user to file
    FILE *file = fopen(USERS_FILE, "a");
    if (file == NULL) {
        unlock_data_files();
        snprintf(response, BUFFER_SIZE, "ERROR|Server error: Cannot access user database");
        send_response(response, client_addr, addr_len);
        return;
    }
    
    fprintf(file, "%s|%s\n", username, password);
    fclose(file);
    
    unlock_data_files();
    
    snprintf(response, BUFFER_SIZE, "SUCCESS|Account created for %s", username);
    send_response(response, client_addr, addr_len);
}

// Processes a LOGIN request by verifying credentials
int authenticate_user(char username[], char password[],
                      struct sockaddr_in *client_addr, socklen_t addr_len) {
    char response[BUFFER_SIZE];
    
    // Sanitize inputs
    sanitize_input(username);
    sanitize_input(password);
    
    // Lock before file access
    lock_data_files();
    
    FILE *file = fopen(USERS_FILE, "r");
    if (file == NULL) {
        unlock_data_files();
        snprintf(response, BUFFER_SIZE, "ERROR|Invalid username or password");
        send_response(response, client_addr, addr_len);
        return 0;
    }
    
    char line[256];
    char stored_username[MAX_USERNAME];
    char stored_password[MAX_PASSWORD];
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Parse username and password (format: username|password)
        char *delimiter = strchr(line, '|');
        if (delimiter) {
            int len = delimiter - line;
            strncpy(stored_username, line, len);
            stored_username[len] = '\0';
            strncpy(stored_password, delimiter + 1, MAX_PASSWORD - 1);
            stored_password[MAX_PASSWORD - 1] = '\0';
            
            if (strcmp(stored_username, username) == 0 && 
                strcmp(stored_password, password) == 0) {
                fclose(file);
                unlock_data_files();
                snprintf(response, BUFFER_SIZE, "SUCCESS|Welcome back %s", username);
                send_response(response, client_addr, addr_len);
                return 1;
            }
        }
    }
    
    fclose(file);
    unlock_data_files();
    
    snprintf(response, BUFFER_SIZE, "ERROR|Invalid username or password");
    send_response(response, client_addr, addr_len);
    return 0;
}

// Processes a DEREGISTER request by removing a user account
void deregister_user(char username[],
                     struct sockaddr_in *client_addr, socklen_t addr_len) {
    char response[BUFFER_SIZE];
    User users[MAX_USERS];
    int user_count = 0;
    
    sanitize_input(username);
    
    // Lock before file access
    lock_data_files();
    
    // Read all users except the one to delete
    FILE *file = fopen(USERS_FILE, "r");
    if (file == NULL) {
        unlock_data_files();
        snprintf(response, BUFFER_SIZE, "ERROR|Server error: Cannot access user database");
        send_response(response, client_addr, addr_len);
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file) && user_count < MAX_USERS) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char *delimiter = strchr(line, '|');
        if (delimiter) {
            int len = delimiter - line;
            char stored_username[MAX_USERNAME];
            strncpy(stored_username, line, len);
            stored_username[len] = '\0';
            
            // Skip the user being deleted
            if (strcmp(stored_username, username) != 0) {
                strncpy(users[user_count].username, stored_username, MAX_USERNAME - 1);
                strncpy(users[user_count].password, delimiter + 1, MAX_PASSWORD - 1);
                users[user_count].username[MAX_USERNAME - 1] = '\0';
                users[user_count].password[MAX_PASSWORD - 1] = '\0';
                user_count++;
            }
        }
    }
    fclose(file);
    
    // Rewrite the file without the deleted user
    file = fopen(USERS_FILE, "w");
    if (file == NULL) {
        unlock_data_files();
        snprintf(response, BUFFER_SIZE, "ERROR|Server error: Cannot update user database");
        send_response(response, client_addr, addr_len);
        return;
    }
    
    for (int i = 0; i < user_count; i++) {
        fprintf(file, "%s|%s\n", users[i].username, users[i].password);
    }
    fclose(file);
    
    unlock_data_files();
    
    snprintf(response, BUFFER_SIZE, "SUCCESS|Account permanently deleted");
    send_response(response, client_addr, addr_len);
}

// Processes a SEARCH request by looking up a target username
void search_user(char username[], char target[],
                 struct sockaddr_in *client_addr, socklen_t addr_len) {
    char response[BUFFER_SIZE];
    
    sanitize_input(username);
    sanitize_input(target);
    
    // Check for self-search
    if (strcmp(username, target) == 0) {
        snprintf(response, BUFFER_SIZE, "ERROR|You cannot search for yourself");
        send_response(response, client_addr, addr_len);
        return;
    }
    
    // Lock before file access
    lock_data_files();
    
    // Check if target exists
    if (!validate_unique_user(target)) {
        unlock_data_files();
        // User exists (validate_unique_user returns 0 if exists)
        snprintf(response, BUFFER_SIZE, "SUCCESS|User %s is registered and available for chat", target);
    } else {
        unlock_data_files();
        snprintf(response, BUFFER_SIZE, "ERROR|User %s not found", target);
    }
    
    send_response(response, client_addr, addr_len);
}

// Processes a LOGOUT request
void logout_user(char username[],
                 struct sockaddr_in *client_addr, socklen_t addr_len) {
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "SUCCESS|Goodbye %s", username);
    send_response(response, client_addr, addr_len);
}
