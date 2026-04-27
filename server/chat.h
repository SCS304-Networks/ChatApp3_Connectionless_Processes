#ifndef CHAT_H
#define CHAT_H

#include <sys/socket.h>
#include <netinet/in.h>

// Processes a SEND request by storing a new message
void send_message(char sender[], char receiver[], char text[],
                  struct sockaddr_in *client_addr, socklen_t addr_len);

// Processes a FETCH request by retrieving conversation messages
void fetch_messages(char sender[], char receiver[],
                    struct sockaddr_in *client_addr, socklen_t addr_len);

// Processes a CONTACTS request by retrieving list of chat partners
void fetch_contacts(char username[],
                    struct sockaddr_in *client_addr, socklen_t addr_len);

// Helper function to get the highest message ID
int get_last_message_id();

#endif // CHAT_H
