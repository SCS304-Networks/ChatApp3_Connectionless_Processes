#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "../shared/network_config.h"
#include "network_server.h"


static int network_mode = NETWORK_MODE_LAN;
static int discovery_socket = -1;
static char local_tailscale_ip[INET_ADDRSTRLEN] = "";
static char local_lan_ip[INET_ADDRSTRLEN] = "";

static int extract_tailscale_ip(const char *status_output, char *ip_buffer,
                                size_t ip_buffer_size) {
  const char *cursor = status_output;

  while (cursor && *cursor) {
    if (strncmp(cursor, "100.", 4) == 0) {
      int a, b, c, d;
      char parsed_ip[INET_ADDRSTRLEN];

      if (sscanf(cursor, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        snprintf(parsed_ip, sizeof(parsed_ip), "%d.%d.%d.%d", a, b, c, d);
        strncpy(ip_buffer, parsed_ip, ip_buffer_size - 1);
        ip_buffer[ip_buffer_size - 1] = '\0';
        return 1;
      }
    }
    cursor++;
  }

  return 0;
}

static int get_tailscale_status(char *output, size_t output_size,
                                char *tailscale_ip, size_t ip_size) {
  FILE *pipe;
  char line[512];
  size_t used = 0;

  output[0] = '\0';
  tailscale_ip[0] = '\0';

  // Try native Linux command first, then Windows executable via WSL interop
  pipe = popen("tailscale status 2>/dev/null || tailscale.exe status 2>/dev/null", "r");
  if (pipe == NULL) {
    return 0;
  }

  while (fgets(line, sizeof(line), pipe) != NULL) {
    size_t line_length = strlen(line);

    if (used + line_length < output_size - 1) {
      strcpy(output + used, line);
      used += line_length;
    }
  }

  pclose(pipe);

  if (used == 0) {
    return 0;
  }

  return extract_tailscale_ip(output, tailscale_ip, ip_size);
}

static int determine_lan_ip(char *ip_buffer, size_t ip_buffer_size) {
    // Allow override via environment variable (useful for WSL2 where the
    // detected IP is the internal VM address, not the Windows host IP)
    const char *override_ip = getenv("SERVER_IP");
    if (override_ip != NULL && strlen(override_ip) > 0) {
        strncpy(ip_buffer, override_ip, ip_buffer_size - 1);
        ip_buffer[ip_buffer_size - 1] = '\0';
        return 1;
    }

    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];
    char fallback_ip[INET_ADDRSTRLEN] = "";

    if (getifaddrs(&ifaddr) == -1) {
        return 0;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));

        // Skip Tailscale addresses — save as fallback only
        if (strncmp(ip, "100.", 4) == 0) {
            if (strlen(fallback_ip) == 0) {
                strncpy(fallback_ip, ip, sizeof(fallback_ip) - 1);
            }
            continue;
        }

        // Prefer 192.168.x.x or 10.x.x.x (real LAN)
        if (strncmp(ip, "192.168.", 8) == 0 || strncmp(ip, "10.", 3) == 0) {
            strncpy(ip_buffer, ip, ip_buffer_size - 1);
            ip_buffer[ip_buffer_size - 1] = '\0';
            freeifaddrs(ifaddr);
            return 1;
        }
    }

    // Fall back to any non-loopback IP if no preferred one found
    if (strlen(fallback_ip) > 0) {
        strncpy(ip_buffer, fallback_ip, ip_buffer_size - 1);
        ip_buffer[ip_buffer_size - 1] = '\0';
        freeifaddrs(ifaddr);
        return 1;
    }

    freeifaddrs(ifaddr);
    return 0;
}

static int setup_discovery_socket() {
  struct sockaddr_in discovery_addr;
  int reuse = 1;

  discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (discovery_socket == -1) {
    return 0;
  }

  setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &reuse,
             sizeof(reuse));

  memset(&discovery_addr, 0, sizeof(discovery_addr));
  discovery_addr.sin_family = AF_INET;
  discovery_addr.sin_port = htons(DISCOVERY_PORT);
  discovery_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(discovery_socket, (struct sockaddr *)&discovery_addr,
           sizeof(discovery_addr)) == -1) {
    close(discovery_socket);
    discovery_socket = -1;
    return 0;
  }

  return 1;
}

int initialize_server_network() {
  char status_output[4096];

  if (get_tailscale_status(status_output, sizeof(status_output),
                           local_tailscale_ip, sizeof(local_tailscale_ip)) &&
      strstr(status_output, "offline") == NULL) {
    network_mode = NETWORK_MODE_TAILSCALE;
    printf("[Network] Tailscale detected and active.\n");
    printf("[Network] Local Tailscale IP: %s\n", local_tailscale_ip);
  } else {
    network_mode = NETWORK_MODE_LAN;
    printf("[Network] Tailscale unavailable or offline.\n");
  }

  if (determine_lan_ip(local_lan_ip, sizeof(local_lan_ip))) {
    printf("[Network] Local LAN IP: %s\n", local_lan_ip);
    if (setup_discovery_socket()) {
      printf("[Network] LAN discovery ready on port %d.\n", DISCOVERY_PORT);
    } else {
      printf("[Network] LAN discovery unavailable.\n");
    }
  } else {
    printf("[Network] LAN IP not found. Clients can use manual IP mode.\n");
  }

  return 1;
}

int get_server_network_mode() { return network_mode; }

int get_discovery_socket() { return discovery_socket; }

void handle_discovery_request(int socket_handle) {
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char buffer[BUFFER_SIZE];
  char response[BUFFER_SIZE];
  int received;

  received = recvfrom(socket_handle, buffer, BUFFER_SIZE - 1, 0,
                      (struct sockaddr *)&client_addr, &client_addr_len);
  if (received <= 0) {
    return;
  }

  buffer[received] = '\0';

  if (strcmp(buffer, DISCOVERY_REQUEST) != 0) {
    return;
  }

  snprintf(response, sizeof(response), "%s|%s|%d", DISCOVERY_RESPONSE_PREFIX,
           local_lan_ip, PORT);

  sendto(socket_handle, response, (int)strlen(response), 0,
         (struct sockaddr *)&client_addr, client_addr_len);
}

void cleanup_server_network() {
  if (discovery_socket != -1) {
    close(discovery_socket);
    discovery_socket = -1;
  }
}
