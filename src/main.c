#include "../libft/libft.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>

unsigned short checksum(void *buf, int len)
{
    unsigned short *data = buf;
    unsigned int sum = 0;

    while (len > 1)
    {
        sum += *data++;
        len -= 2;
    }
    if (len == 1)
        sum += *(unsigned char *)data;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >>16);
    return (~sum);
}

int main(int argc, char **argv)
{
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *res;
    
    if (argc < 2)
        return (printf("Not enough arguments."), 1);
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd == -1)
        return (perror("sockfd failed"), 1);

    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    int addrerror = getaddrinfo(argv[1], NULL, &hints, &res);
    if (addrerror)
    {
        fprintf(stderr, "ft_ping: %s\n", gai_strerror(addrerror));
        return (1);
    }

    struct sockaddr_in *sockaddr;
    sockaddr = (struct sockaddr_in *)res->ai_addr;
    printf("Resolved: %s\n", inet_ntoa(sockaddr->sin_addr));

    struct icmphdr echo_req;
    echo_req.code = 0;
    echo_req.type = ICMP_ECHO;
    echo_req.un.echo.id = getpid() & 0xFFFF;
    echo_req.un.echo.sequence = 1;
    echo_req.checksum = 0;
    echo_req.checksum = checksum(&echo_req, sizeof(echo_req));

    if (sendto(sockfd, &echo_req, sizeof(echo_req), 0, (struct sockaddr *)sockaddr, sizeof(*sockaddr)) < 0)
    {
        perror("sendto");
        return (1);
    }
    printf("Sent ICMP echo request (8 bytes)\n");

    freeaddrinfo(res);
    close(sockfd);
    return (0);
}
