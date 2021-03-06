#include "server.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/**
 * Extracts parts of request from naked buffer
 *
 * `buffer`: naked buffer from socket
 * `request`: output request
 */
static void extract_from_raw(char const *buffer, request_t *request) {
  char version[16] = {0};
  sscanf(buffer, "%s %s %s", request->method, request->endpoint, version);

  char const delim[] = "\r\n\r\n";

  char *body = strstr(buffer, delim);
  if (body != NULL) {
    body += sizeof(delim) - 1;
    printf("Body: '%s'\r\n", body);
    strncpy(request->body, body, strlen(body));
  }
}

static method_t str_to_method(char const *str) {
  if (strcmp(str, "GET") == 0) {
    return GET;
  } else if (strcmp(str, "POST") == 0) {
    return POST;
  } else {
    return UNKN;
  }
}

/**
 * Thread Function, called when new connections are made
 *
 * `args`: pointer to integer with client socket file descriptor
 * @return void* Always NULL
 */
static void *new_client_handler(void *args) {
  int sock_client = ((handler_args_t *)args)->sock_fd;
  server_t *pserver = ((handler_args_t *)args)->pserver;

  char buffer[LINE_MAX] = {0};
  int valread = read(sock_client, buffer, sizeof(buffer));
  request_t request = {0};
  extract_from_raw(buffer, &request);
  method_t method = str_to_method(request.method);

  char response[128] = {0};
  /* Loop through configured endpoints, if match, call callback */
  for (int i = 0; i < pserver->endpoint_count; ++i) {
    if (strcmp(request.endpoint, pserver->endpoints[i].name) == 0) {
      pserver->endpoints[i].callback(method, request.body, response);
    }
  }

  char ok[255] = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\n";

  if (strlen(response) > 0) {
    strcat(ok, response);
  }

  send(sock_client, ok, strlen(ok), 0);
  close(sock_client);
  return NULL;
}

server_e_t server_init(server_t *server) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("Failed to CREATE Socket\r\n");
    return SERVER_FAIL;
  }
  server->_socket.fd = sockfd;
  struct sockaddr_in addr_in = {.sin_family = AF_INET,
                                .sin_addr = {.s_addr = INADDR_ANY},
                                .sin_port = htons(server->port)};
  server->_socket.addr = addr_in;
  struct sockaddr *const paddr = (struct sockaddr *)&server->_socket.addr;
  server->_socket.paddr = paddr;
  uint32_t addrlen = sizeof(server->_socket.addr);
  server->_socket.addrlen = addrlen;
  int binded = bind(sockfd, paddr, addrlen);
  if (binded < 0) {
    perror("bind");
    return SERVER_FAIL;
  }

  return SERVER_OK;
}

server_e_t server_listen(server_t *server) {

  int listening = listen(server->_socket.fd, 2);
  if (listening < 0) {
    perror("listen");
    return SERVER_FAIL;
  }

  while (true) {
    int sock_client = accept(server->_socket.fd, server->_socket.paddr,
                             &server->_socket.addrlen);
    if (sock_client < 0) {
      perror("accept");
      return SERVER_FAIL;
    }

    pthread_t client_thread = {0};
    handler_args_t args = {.sock_fd = sock_client, .pserver = server};
    pthread_create(&client_thread, NULL, new_client_handler, &args);
  }

  return SERVER_OK;
}
