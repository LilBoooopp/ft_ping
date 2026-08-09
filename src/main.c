#include "../libft/libft.h"
#include <asm-generic/errno.h>
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
        return (printf("Not enough arguments.\n"), 1);
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

    int seq = 1;

    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    while (1)
    {
        struct icmphdr echo_req;
        echo_req.code = 0;
        echo_req.type = ICMP_ECHO;
        echo_req.un.echo.id = getpid() & 0xFFFF;
        echo_req.un.echo.sequence = seq;
        echo_req.checksum = 0;
        echo_req.checksum = checksum(&echo_req, sizeof(echo_req));

        if (sendto(sockfd, &echo_req, sizeof(echo_req), 0, (struct sockaddr *)sockaddr, sizeof(*sockaddr)) < 0)
        {
            perror("sendto");
            return (1);
        }

        // RECEIVE
        char recv_buf[64];
        struct sockaddr_in reply_addr;
        socklen_t addr_len = sizeof(reply_addr);
        struct timeval start, end;
        ssize_t bytes;

        gettimeofday(&start, NULL);
        bytes = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&reply_addr, &addr_len);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                ; // packet lost
            else
            {
                perror("recvfrom");
                return (1);
            }
        }
        gettimeofday(&end, NULL);

        struct iphdr *ip = (struct iphdr *)recv_buf;
        unsigned int ip_hdr_len = ip->ihl * 4;
        struct icmphdr *reply = (struct icmphdr *)(recv_buf + ip_hdr_len);

        if (reply->type != ICMP_ECHOREPLY || reply->un.echo.id != (getpid() & 0xFFFF))
        {
            fprintf(stderr, "Wrong reply\n");
            return (1);
        }

        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;

        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.1f ms\n", (int)(bytes - ip_hdr_len), inet_ntoa(reply_addr.sin_addr), reply->un.echo.sequence, ip->ttl, rtt);

        seq++;
        sleep(1);
    }

    freeaddrinfo(res);
    close(sockfd);
    return (0);
}
