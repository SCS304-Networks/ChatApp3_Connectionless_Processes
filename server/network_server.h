#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

int initialize_server_network();
int get_server_network_mode();
int get_discovery_socket();
void handle_discovery_request(int discovery_socket);
void cleanup_server_network();

#endif // NETWORK_SERVER_H
