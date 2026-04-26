#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

#define MAXDATASIZE 4096

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// post() sends a POST request to a given host (eg, "http://example.com"),
// and returns a pointer to a response object.
// The caller is responsible for freeing the response.
HTTPResponse *post(char *host, char *port, char* body, size_t body_len, FILE *output_stream, FILE *error_stream){
    HTTPResponse *resp = NULL;
    int sockfd;
    int numbytes;
    struct addrinfo hints, *servinfo, *p;
    int status;
    char s[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE]; // whatever

    // zero out hints
    void *memset_res = memset(&hints, 0, sizeof(hints));
    if (NULL == memset_res) {
        fprintf(output_stream, "Failed to zero out memory for hints\n");
        return NULL;
    }
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(error_stream, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    // loop through results and connect to one
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        inet_ntop(
            p->ai_family,
            get_in_addr((struct sockaddr *)p->ai_addr),
            s,
            sizeof(s)
        );
        fprintf(output_stream, "Attempting connection to %s\n!", s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(error_stream, "Failed to connect to %s :( \n", host);
        return NULL;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    fprintf(output_stream, "Connected to %s\n", s);

    freeaddrinfo(servinfo);

    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        return NULL;
    }

    buf[numbytes] = '\0';

    close(sockfd);

    return resp; // TODO: return http response, probably my own struct
}
