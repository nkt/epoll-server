#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAX_EVENTS SOMAXCONN
#define READ_BUFFER_SIZE 512

int create_socket(const char *port)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  // IPv4
  hints.ai_family = AF_INET;
  // TCP socket
  hints.ai_socktype = SOCK_STREAM;
  // The socket address will be used in a call to the bind function
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *result;
  if (getaddrinfo(NULL, port, &hints, &result) != 0) {
    perror("getaddrinfo");
    return -1;
  }

  struct addrinfo *it;
  for (it = result; it != NULL; it = it->ai_next) {
    int sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (sock == -1) {
      continue;
    }

    if (bind(sock, it->ai_addr, it->ai_addrlen) == 0) {
      freeaddrinfo(result);
      return sock;
    }

    close(sock);
  }

  fprintf (stderr, "create_socket: failed\n");
  return -1;
}

int make_socket_non_blocking(int sock)
{
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    return -1;
  }

  flags |= O_NONBLOCK;
  if (fcntl(sock, F_SETFL, flags) == -1) {
    perror("fcntl");
    return -1;
  }

  return 0;
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "Usage: %s [port]\n", argv[0]);
    return 1;
  }

  int sock = create_socket(argv[1]);

  if (sock == -1) {
    return 1;
  }

  if (make_socket_non_blocking(sock) == -1) {
    return 1;
  }

  if (listen(sock, MAX_EVENTS) == -1) {
    perror("listen");
    return 1;
  }

  int efd = epoll_create1(0);
  if (efd == -1) {
    perror("epoll_create");
    return 1;
  }

  struct epoll_event event;
  event.data.fd = sock;
  event.events = EPOLLIN | EPOLLET;

  if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &event) == -1) {
    perror("epoll_ctl");
    return 1;
  }

  struct epoll_event *events = calloc(MAX_EVENTS, sizeof(event));
  for (;;) {
    int count, i;
    count = epoll_wait(efd, events, MAX_EVENTS, -1);
    for (i = 0; i < count; i++) {
      if (
        (events[i].events & EPOLLERR)
      ) {
        perror("epoll_wait");
        close(events[i].data.fd);
	      continue;
      } else if (sock == events[i].data.fd) {
        // There are new incomming connections!
        for (;;) {
          struct sockaddr in_addr;
          socklen_t in_len;
          int in_sock = accept(sock, &in_addr, &in_len);
          if (in_sock == -1) {
            // Otherwise there are no more incomming connections.
            if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
              perror("accept");
            }
            break;
          }

          if (make_socket_non_blocking(in_sock) == -1) {
            return 1;
          }

          // Add sock to epoll list
          event.data.fd = in_sock;
          event.events = EPOLLIN | EPOLLET;
          if (epoll_ctl(efd, EPOLL_CTL_ADD, in_sock, &event) == -1) {
            perror("epoll_ctl");
            return 1;
          }
        }
        continue;
      } else {
        // We got some data for read.
        int done = 0;
        for (;;) {
          char buf[READ_BUFFER_SIZE];
          ssize_t count = read(events[i].data.fd, buf, sizeof(buf));
          if (count == -1) {
            // Otherwise we've read all data.
            if (errno != EAGAIN) {
              perror("read");
              done = 1;
            }
            break;
          } else if (count == 0) {
            // Remote closed connection.
            done = 1;
            break;
          }

          // Output received data
          if (write(STDOUT_FILENO, &buf, count) == -1) {
            perror("write");
            return 1;
          }
        }

        if (done) {
          close(events[i].data.fd);
        }
      }
    }
  }

  close(sock);
  return 0;
}
