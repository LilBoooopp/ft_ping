#include "../libft/libft.h"
#include <asm-generic/errno.h>
#include <math.h>
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

static volatile sig_atomic_t keep_running = 1;
static int sent_count = 0;
static int recv_count = 0;
static double min_rtt = 999999.0;
static double max_rtt = 0.0;
static double sum_rtt = 0.0;
static double sum_rtt_sq = 0.0;

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

static void sig_handler(int sig)
{
    (void)sig;
    keep_running = 0;
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
    printf("PING %s (%s) 56(84) bytes of data.\n", argv[1], inet_ntoa(sockaddr->sin_addr));

    int seq = 1;

    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct timeval prog_start;
    gettimeofday(&prog_start, NULL);

    signal(SIGINT, sig_handler);
    while (keep_running)
    {
        sleep(1);
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
        sent_count++;

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
                continue; // packet lost
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
            continue;

        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;

        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.1f ms\n", (int)(bytes - ip_hdr_len), inet_ntoa(reply_addr.sin_addr), reply->un.echo.sequence, ip->ttl, rtt);

        seq++;
        recv_count++;
        if (rtt < min_rtt) min_rtt = rtt;
        if (rtt > max_rtt) max_rtt = rtt;
        sum_rtt += rtt;
        sum_rtt_sq += rtt * rtt;
    }

    double avg = 0.0, mdev = 0.0;
    if (recv_count > 0)
    {
        avg = sum_rtt / recv_count;
        mdev = sqrt(sum_rtt_sq / recv_count - avg * avg);
    }

    struct timeval prog_end;
    gettimeofday(&prog_end, NULL);
    double total_time = (prog_end.tv_sec - prog_start.tv_sec) * 1000.0 + (prog_end.tv_usec - prog_start.tv_usec) / 1000.0;
    double loss = sent_count > 0 ? (sent_count - recv_count) * 100.0 / sent_count : 0.0;

    printf("\n--- %s ping statistics ---\n", argv[1]);
    printf("%d packets transmitted, %d received, %d%% packet loss, time %.1f ms\n", sent_count, recv_count, (int)loss, total_time);
    printf("rtt min/avg/max/mdev = %.1f/%.1f/%.1f/%.1f ms\n", min_rtt, avg, max_rtt, mdev);

    freeaddrinfo(res);
    close(sockfd);
    return (0);
}

