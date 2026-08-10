*This project has been created as part of the 42 curriculum by cbopp*

# ft_ping

Recoding the `ping` command -- sending ICMP ECHO_REQUEST packets to network hosts.

## Build
```bash
make
```

Requires `libft` (included as a submodule). The binary is `ft_ping`.

## Usage

```bash
sudo ./ft_ping [options] <destination>
```

| Flag | Argument | Description |
|------|----------|-------------|
| `-v` |          | Verbose - print ICMP errors from intermediate routers (TTL exceeded, unreachable, etc.) |
| `-?` |          | Show usage help |
| `-f` |          | Flood ping - send as fast as possible, show dots |
| `-n` |          | Numeric output only (default, no reverse DNS) |
| `-s` | `<size>` | Packet payload size in bytes (default: 56) |
| `-w` | `<sec>` | Global deadline - stop after N seconds |
| `-W` | `<sec>` | Per-packet timeout in seconds (default: 1) |
| `--ttl` | `<n>` | Ste IP Time-to-Live |

### Examples

```bash
# Basic ping
sudo ./ft_ping google.com

# Verbose with ICMP errors
sudo ./ft_ping -v google.com

# Custom payload size
sudo ./ft_ping -s 1024 google.com

# Flood with 1-second per-packet timeout
sudo ./ft_ping -f -W 1 google.com

# Stop after 5 seconds
sudo ./ft_ping -w 5 google.com
```

### Example output

```bash
$ sudo ./ft_ping google.com
PING google.com (142.250.202.14) 56(84) bytes of data.
8 bytes from 142.250.202.14: icmp_seq=1 ttl=115 time=9.3 ms
8 bytes from 142.250.202.14: icmp_seq=2 ttl=115 time=10.3 ms
8 bytes from 142.250.202.14: icmp_seq=3 ttl=115 time=10.7 ms
^C
--- google.com ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2320ms
rtt min/avg/max/mdev = 9.320/10.091/10.700/0.575 ms
```

### Verbose mode

```bash
$ sudo ./ft_ping -v google.com
```

Prints ICMP errors like `Time to live exceeded` or `Destination Host Unreachable` from intermediate routers instead of silently discarding them.

## How it works

- Opens a **raw ICMP** (`SOCK_RAW`, `IPPROTO_ICMP`) - requires root
- Builds ICMP echo request headers with a custom **checksum** (one's complement sum)
- Resolves hostnames via `getaddrinfo()`
- Receives raw IP packets, skips the IP header, and parses the ICMP echo reply
- Computes **round-trip time** with microsecond precision using `gettimeofday()`
- Handles `SIGINT` (Ctrl+C) for clean shutdown with statistics
