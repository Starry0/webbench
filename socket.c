#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>


int Socket(const char *host, int clientPort) {
    int sock;
    unsigned long inaddr;
    struct sockaddr_in adr;
    struct hostent *hp;

    memset(&adr, 0, sizeof(adr));
    adr.sin_family = AF_INET;

    inaddr = inet_addr(host);
    if(inaddr != INADDR_NONE) {
        memcpy(&adr.sin_addr, &inaddr, sizeof(inaddr));
    } else {
        hp = gethostbyname(host);
        if(hp == NULL) return -1;
        memcpy(&adr.sin_addr, hp->h_addr, sizeof(hp->h_length));
    }
    adr.sin_port = htons(clientPort);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) return sock;
    if(connect(sock, (struct sockaddr*)&adr, sizeof(adr)) < 0)
        return -1;
    return sock;
}