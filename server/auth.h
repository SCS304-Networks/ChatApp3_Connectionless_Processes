#ifndef AUTH_H
#define AUTH_H

#include <sys/socket.h>
#include <netinet/in.h>

// Processes a REGISTER request by creating a new user account
void register_user(char username[], char password[],
                   struct sockaddr_in *client_addr, socklen_t addr_len);

// Processes a LOGIN request by verifying credentials
int authenticate_user(char username[], char password[],
                      struct sockaddr_in *client_addr, socklen_t addr_len);

// Checks if a username already exists in the registry
int validate_unique_user(char username[]);

// Processes a DEREGISTER request by removing a user account
void deregister_user(char username[],
                     struct sockaddr_in *client_addr, socklen_t addr_len);

// Processes a SEARCH request by looking up a target username
void search_user(char username[], char target[],
                 struct sockaddr_in *client_addr, socklen_t addr_len);

// Processes a LOGOUT request
void logout_user(char username[],
                 struct sockaddr_in *client_addr, socklen_t addr_len);

#endif // AUTH_H
