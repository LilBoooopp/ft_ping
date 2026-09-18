#include "../libft/libft.h"
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
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
#include <time.h>

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

static const char *icmp_error_msg(int type, int code)
{
    if (type == ICMP_TIME_EXCEEDED && code == ICMP_EXC_TTL)
        return ("Time to live exceeded");
    if (type == ICMP_DEST_UNREACH)
    {
        if (code == ICMP_NET_UNREACH)
            return ("Destination Net Unreachable");
        if (code == ICMP_HOST_UNREACH)
            return ("Destination Host Unreachable");
        return ("Destination Unreachable");
    }
    return ("Unknown ICMP error");
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
    
    int verbose = 0;
    int opt_idx = 1;
    int ttl = 0;
    int deadline = 0;
    int global_deadline = 0;
    int flood = 0;
    int datasize = 56;

    while (opt_idx < argc && argv[opt_idx][0] == '-')
    {
        if (ft_strcmp(argv[opt_idx], "-v") == 0)
            verbose = 1;
        else if (ft_strcmp(argv[opt_idx], "-?") == 0)
        {
            printf("Usage: ft_ping [-v] [-?] <destination>\n");
            return (0);
        }
        else if (ft_strcmp(argv[opt_idx], "--ttl") == 0)
        {
            opt_idx++;
            if (opt_idx >= argc || !ft_isdigit(argv[opt_idx][0]))
                return (printf("ft_ping: option '--ttl' requires an arugment\n"), 1);
            ttl = ft_atoi(argv[opt_idx]);
        }
        else if (ft_strcmp(argv[opt_idx], "-W") == 0)
        {
            opt_idx++;
            if (opt_idx >= argc || !ft_isdigit(argv[opt_idx][0]))
                return (printf("ft_ping: option '-W' requires a numeric argument\n"), 1);
            deadline = ft_atoi(argv[opt_idx]);
        }
        else if (ft_strcmp(argv[opt_idx], "-w") == 0)
        {
            opt_idx++;
            if (opt_idx >= argc || !ft_isdigit(argv[opt_idx][0]))
                return (printf("ft_ping: option '-w' requires a numeric argument\n"), 1);
            global_deadline = ft_atoi(argv[opt_idx]);
        }
        else if (ft_strcmp(argv[opt_idx], "-s") == 0)
{
            opt_idx++;
            if (opt_idx >= argc || !ft_isdigit(argv[opt_idx][0]))
                return (printf("ft_ping: option '-s' requires a numeric argument\n"), 1);
            datasize = ft_atoi(argv[opt_idx]);
        }
        else if (ft_strcmp(argv[opt_idx], "-f") == 0)
            flood = 1;
        else if (ft_strcmp(argv[opt_idx], "-n") == 0)
            ;
        else
        {
            fprintf(stderr, "ft_ping: invalid option -- '%s'\n", argv[opt_idx]);
            return (1);
        }
        opt_idx++;
    }
    if (opt_idx >= argc)
        return (printf("ft_ping: missing host operand\n"), 1);
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd == -1)
        return (perror("sockfd failed"), 1);


    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    int addrerror = getaddrinfo(argv[opt_idx], NULL, &hints, &res);
    if (addrerror)
    {
        fprintf(stderr, "ft_ping: %s\n", gai_strerror(addrerror));
        return (1);
    }

    struct sockaddr_in *sockaddr;
    sockaddr = (struct sockaddr_in *)res->ai_addr;
    printf("PING %s (%s) %d(%d) bytes of data.\n", argv[opt_idx], inet_ntoa(sockaddr->sin_addr), datasize, (int)(datasize + sizeof(struct icmphdr) + sizeof(struct iphdr)));

    int seq = 1;

    if (deadline > 0)
    {
        struct timeval tv = { .tv_sec = deadline, .tv_usec = 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    else
    {
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    if (ttl > 0)
        setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    struct timeval prog_start;
    gettimeofday(&prog_start, NULL);

    time_t stop_time = 0;
    if (global_deadline > 0)
        stop_time = prog_start.tv_sec + global_deadline;

    signal(SIGINT, sig_handler);

    int packet_size = sizeof(struct icmphdr) + datasize;
    int recv_size = sizeof(struct iphdr) + packet_size + 64;

    char *send_buf = ft_calloc(1, packet_size);
    char *recv_buf = ft_calloc(1, recv_size);

    while (keep_running)
    {
        struct icmphdr *icmp = (struct icmphdr *)send_buf;
        icmp->code = 0;
        icmp->type = ICMP_ECHO;
        icmp->un.echo.id = getpid() & 0xFFFF;
        icmp->un.echo.sequence = seq;
        icmp->checksum = 0;
        icmp->checksum = checksum(send_buf, packet_size);
        seq++;

        if (sendto(sockfd, send_buf, packet_size, 0, (struct sockaddr *)sockaddr, sizeof(*sockaddr)) < 0)
        {
            perror("sendto");
            return (1);
        }
        sent_count++;
        if (flood)
            write(1, ".", 1);

        // RECEIVE
        struct sockaddr_in reply_addr;
        socklen_t addr_len = sizeof(reply_addr);
        struct timeval start, end;
        ssize_t bytes;

        gettimeofday(&start, NULL);
        bytes = recvfrom(sockfd, recv_buf, recv_size, 0, (struct sockaddr *)&reply_addr, &addr_len);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                continue; // packet lost
            perror("recvfrom");
            return (1);
        }
        gettimeofday(&end, NULL);

        struct iphdr *ip = (struct iphdr *)recv_buf;
        unsigned int ip_hdr_len = ip->ihl * 4;
        struct icmphdr *reply = (struct icmphdr *)(recv_buf + ip_hdr_len);

        if (reply->type != ICMP_ECHOREPLY || reply->un.echo.id != (getpid() & 0xFFFF))
        {
            if (verbose)
            {
                struct iphdr *inner_ip = (struct iphdr *)((char *)reply + 8);
                unsigned int inner_ip_len = inner_ip->ihl * 4;
                struct icmphdr *inner_icmp = (struct icmphdr *)((char *)inner_ip + inner_ip_len);
                
                printf("%d bytes from %s: icmp_seq=%d %s\n", (int)(bytes - ip_hdr_len), inet_ntoa(reply_addr.sin_addr), inner_icmp->un.echo.sequence, icmp_error_msg(reply->type, reply->code));
            }
            continue;
        }

        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        char host[NI_MAXHOST];
        if (flood)
            write(1, "\b \b", 3);
        else
        {
            if (getnameinfo((struct sockaddr *)&reply_addr, sizeof(reply_addr), host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0)
                printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.1f ms\n", (int)(bytes - ip_hdr_len), inet_ntoa(reply_addr.sin_addr), reply->un.echo.sequence, ip->ttl, rtt);
        }

        recv_count++;
        if (rtt < min_rtt) min_rtt = rtt;
        if (rtt > max_rtt) max_rtt = rtt;
        sum_rtt += rtt;
        sum_rtt_sq += rtt * rtt;

        if (!keep_running)
            break;
        if (global_deadline > 0 && time(NULL) >= stop_time)
            break;
        if (!flood)
            usleep(1000000);
    }

    free(send_buf);
    free(recv_buf);
    double avg = 0.0, mdev = 0.0;
    if (recv_count > 0)
    {
        avg = sum_rtt / recv_count;
        mdev = sqrt(sum_rtt_sq / recv_count - avg * avg);
    }
    else
        min_rtt = 0.0;

    struct timeval prog_end;
    gettimeofday(&prog_end, NULL);
    double total_time = (prog_end.tv_sec - prog_start.tv_sec) * 1000.0 + (prog_end.tv_usec - prog_start.tv_usec) / 1000.0;
    double loss = sent_count > 0 ? (sent_count - recv_count) * 100.0 / sent_count : 0.0;

    printf("\n--- %s ping statistics ---\n", argv[opt_idx]);
    printf("%d packets transmitted, %d received, %d%% packet loss, time %.0fms\n", sent_count, recv_count, (int)loss, total_time);
    printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", min_rtt, avg, max_rtt, mdev);

    freeaddrinfo(res);
    close(sockfd);
    return (0);
}

