#!/usr/bin/env bash
#
# test_ft_ping.sh — test suite for the ft_ping 42 project
#
# Covers:
#   Mandatory: -? usage, -v verbose, plain IPv4, hostname resolution,
#              FQDN handling (no per-packet reverse DNS), unknown-host
#              errors, and crash-safety on malformed input.
#   Bonus:     --ttl, -f, -n, -s, -w, -W
#
# Usage:
#   ./test_ft_ping.sh [path-to-binary]
#
# The binary needs CAP_NET_RAW to open a raw socket. Either:
#   sudo setcap cap_net_raw+ep ./ft_ping   (then run this script normally)
# or:
#   sudo ./test_ft_ping.sh
#
# Override TEST_FQDN / BLACKHOLE_IP as env vars if the defaults don't
# suit your VM's network (e.g. an isolated NAT with no internet route).

set -u

BINARY="${1:-./ft_ping}"
LOCAL_IP="127.0.0.1"
LOCAL_HOST="localhost"
TEST_FQDN="${TEST_FQDN:-google.com}"
BAD_HOST="this-host-should-not-resolve.invalid"
BLACKHOLE_IP="${BLACKHOLE_IP:-10.255.255.1}"

PASS=0
FAIL=0
SKIP=0

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'

pass() { PASS=$((PASS + 1)); printf "${GREEN}PASS${NC}  %s\n" "$1"; }
fail() { FAIL=$((FAIL + 1)); printf "${RED}FAIL${NC}  %s\n" "$1"; [ -n "${2:-}" ] && printf "        %s\n" "$2"; }
skip_t() { SKIP=$((SKIP + 1)); printf "${YELLOW}SKIP${NC}  %s\n" "$1"; }
section() { printf "\n== %s ==\n" "$1"; }

# bash reports a process killed by signal N as exit code 128+N.
# 139=SIGSEGV 134=SIGABRT 135=SIGBUS 136=SIGFPE 132=SIGILL
is_crash_code() {
    case "$1" in
        139 | 134 | 135 | 136 | 132) return 0 ;;
        *) return 1 ;;
    esac
}

# run_ping DURATION ARGS...
# ft_ping loops until interrupted (like real ping), so tests send SIGINT
# after DURATION seconds — the program's documented clean-shutdown path —
# with a SIGKILL 3s later as a safety net if that handling is broken.
run_ping() {
    local dur=$1
    shift
    timeout -k 3 -s INT "$dur" "$BINARY" "$@" 2>&1
}

# --- sanity checks -------------------------------------------------------

if [ ! -x "$BINARY" ]; then
    echo "error: '$BINARY' not found or not executable. Build with 'make' first."
    exit 1
fi

perm_test=$(timeout -s INT 1 "$BINARY" "$LOCAL_IP" 2>&1)
if echo "$perm_test" | grep -qi "not permitted"; then
    echo "error: $BINARY can't open a raw socket."
    echo "  run:  sudo setcap cap_net_raw+ep $BINARY"
    echo "  or:   sudo $0 $BINARY"
    exit 1
fi

# --- mandatory part --------------------------------------------------------

test_ipv4() {
    local out; out=$(run_ping 2 "$LOCAL_IP")
    if is_crash_code "$?"; then
        fail "IPv4 ping to $LOCAL_IP" "process crashed (exit $?)"
        return
    fi
    if echo "$out" | grep -q "^PING $LOCAL_IP" \
        && echo "$out" | grep -qE "[0-9]+ bytes from $LOCAL_IP" \
        && echo "$out" | grep -q "ping statistics" \
        && echo "$out" | grep -q "packets transmitted"; then
        pass "IPv4 ping to $LOCAL_IP produces correctly formatted output"
    else
        fail "IPv4 ping to $LOCAL_IP" "$out"
    fi
}

test_hostname() {
    local out; out=$(run_ping 2 "$LOCAL_HOST")
    if is_crash_code "$?"; then
        fail "hostname ping to $LOCAL_HOST" "process crashed (exit $?)"
        return
    fi
    if echo "$out" | grep -qE "^PING $LOCAL_HOST \($LOCAL_IP\)"; then
        pass "hostname '$LOCAL_HOST' resolves and shows in the PING header"
    else
        fail "hostname ping to $LOCAL_HOST" "$out"
    fi
}

test_fqdn() {
    if ! getent hosts "$TEST_FQDN" >/dev/null 2>&1; then
        skip_t "FQDN ping to $TEST_FQDN (no network/DNS in this environment)"
        return
    fi
    local out; out=$(run_ping 3 "$TEST_FQDN")
    if is_crash_code "$?"; then
        fail "FQDN ping to $TEST_FQDN" "process crashed (exit $?)"
        return
    fi
    if ! echo "$out" | grep -qE "^PING $TEST_FQDN \("; then
        fail "FQDN ping to $TEST_FQDN" "no PING header with resolved address: $out"
        return
    fi
    if echo "$out" | grep "bytes from" | grep -qE "bytes from [a-zA-Z]"; then
        fail "FQDN ping to $TEST_FQDN" "reply line shows a resolved name instead of a numeric address"
    else
        pass "FQDN '$TEST_FQDN' resolves once; replies stay numeric (no per-packet reverse DNS)"
    fi
}

test_usage_flag() {
    local out; out=$(timeout 2 "$BINARY" -? 2>&1)
    if is_crash_code "$?"; then
        fail "-? flag" "process crashed (exit $?)"
        return
    fi
    if [ -n "$out" ]; then
        pass "-? prints usage output"
    else
        fail "-? flag" "no output produced"
    fi
}

test_verbose_flag_basic() {
    local out; out=$(run_ping 2 -v "$LOCAL_IP")
    if is_crash_code "$?"; then
        fail "-v flag on a successful ping" "process crashed (exit $?)"
        return
    fi
    if echo "$out" | grep -q "^PING $LOCAL_IP"; then
        pass "-v works on a normal successful ping without changing the happy path"
    else
        fail "-v flag on a successful ping" "$out"
    fi
}

test_unknown_host() {
    local out; out=$(timeout 3 "$BINARY" "$BAD_HOST" 2>&1)
    local code=$?
    if is_crash_code "$code"; then
        fail "unknown host handling" "process crashed (exit $code) instead of reporting an error"
        return
    fi
    if [ "$code" -ne 0 ] && [ -n "$out" ]; then
        pass "unknown host '$BAD_HOST' errors out cleanly (exit $code)"
    else
        fail "unknown host handling" "expected a non-zero exit with an error message, got exit $code: $out"
    fi
}

test_no_crash_edge_cases() {
    local cases=(
        ""             # no arguments at all
        "-v"           # flag with no destination
        "--bogus-flag" # unrecognized flag
        "-s"           # -s with no value
    )
    local all_ok=1
    for args in "${cases[@]}"; do
        # shellcheck disable=SC2086
        timeout 2 "$BINARY" $args >/dev/null 2>&1
        if is_crash_code "$?"; then
            fail "edge case: '$args'" "process crashed (exit $?)"
            all_ok=0
        fi
    done
    [ "$all_ok" -eq 1 ] && pass "no crashes on missing or malformed arguments"
}

# --- bonus flags -------------------------------------------------------

test_bonus_ttl() {
    local out; out=$(run_ping 2 --ttl 64 "$LOCAL_IP")
    if is_crash_code "$?"; then
        fail "--ttl 64 to $LOCAL_IP" "process crashed"
        return
    fi
    if echo "$out" | grep -q "^PING $LOCAL_IP"; then
        pass "--ttl 64 behaves like a normal ping"
    else
        fail "--ttl 64 to $LOCAL_IP" "$out"
    fi

    if getent hosts "$TEST_FQDN" >/dev/null 2>&1; then
        out=$(run_ping 3 -v --ttl 1 "$TEST_FQDN")
        if is_crash_code "$?"; then
            fail "--ttl 1 -v forcing a router error" "process crashed"
        elif echo "$out" | grep -qiE "time.to.live|ttl.exceed|exceeded"; then
            pass "-v reports a TTL-exceeded error from an intermediate router"
        else
            fail "--ttl 1 -v forcing a router error" "no TTL-exceeded message seen: $out"
        fi
    else
        skip_t "--ttl 1 -v router-error test (no network available)"
    fi
}

test_bonus_flood() {
    local out; out=$(run_ping 2 -f "$LOCAL_IP")
    if is_crash_code "$?"; then
        fail "-f flood mode" "process crashed"
        return
    fi
    local sent
    sent=$(echo "$out" | grep -oE "[0-9]+ packets transmitted" | grep -oE "[0-9]+")
    if [ -n "$sent" ] && [ "$sent" -gt 20 ]; then
        pass "-f sends noticeably faster than the default 1/sec (~$sent packets in 2s)"
    else
        fail "-f flood mode" "only '${sent:-0}' packets sent in 2s, expected a flood"
    fi
}

test_bonus_numeric() {
    local out; out=$(run_ping 2 -n "$LOCAL_HOST")
    if is_crash_code "$?"; then
        fail "-n flag" "process crashed"
        return
    fi
    if echo "$out" | grep -qE "^PING $LOCAL_HOST \($LOCAL_IP\)" \
        && ! echo "$out" | grep "bytes from" | grep -qE "bytes from [a-zA-Z]"; then
        pass "-n produces numeric-only output for hostname '$LOCAL_HOST'"
    else
        fail "-n flag" "$out"
    fi
}

test_bonus_size() {
    local size=100
    local icmp_total=$((size + 8))   # payload + 8-byte ICMP header
    local ip_total=$((size + 28))    # payload + ICMP header + 20-byte IP header
    local out; out=$(run_ping 2 -s "$size" "$LOCAL_IP")
    if is_crash_code "$?"; then
        fail "-s $size" "process crashed"
        return
    fi
    if echo "$out" | grep -qE "${size}\(${ip_total}\) bytes of data" \
        && echo "$out" | grep -qE "${icmp_total} bytes from"; then
        pass "-s $size changes the reported packet size accordingly"
    else
        fail "-s $size" "expected $size/$ip_total in the header and $icmp_total in reply lines: $out"
    fi
}

test_bonus_deadline() {
    local dur=3
    local start end elapsed
    start=$(date +%s)
    timeout $((dur + 5)) "$BINARY" -w "$dur" "$LOCAL_IP" >/dev/null 2>&1
    local code=$?
    end=$(date +%s)
    elapsed=$((end - start))
    if is_crash_code "$code"; then
        fail "-w $dur deadline" "process crashed"
    elif [ "$elapsed" -ge "$dur" ] && [ "$elapsed" -le "$((dur + 2))" ]; then
        pass "-w $dur makes the program exit on its own after ~${elapsed}s"
    else
        fail "-w $dur deadline" "expected exit around ${dur}s, took ${elapsed}s"
    fi
}

test_bonus_per_packet_timeout() {
    local w=1
    local out; out=$(run_ping 4 -W "$w" "$BLACKHOLE_IP")
    if is_crash_code "$?"; then
        fail "-W $w per-packet timeout" "process crashed"
        return
    fi
    if echo "$out" | grep -qE "100% packet loss"; then
        pass "-W $w correctly times out unanswered requests (100% loss reported)"
    else
        fail "-W $w per-packet timeout" "$out"
    fi
}

# --- run everything ------------------------------------------------------

section "Mandatory part"
test_ipv4
test_hostname
test_fqdn
test_usage_flag
test_verbose_flag_basic
test_unknown_host
test_no_crash_edge_cases

section "Bonus flags"
test_bonus_ttl
test_bonus_flood
test_bonus_numeric
test_bonus_size
test_bonus_deadline
test_bonus_per_packet_timeout

printf "\n=========================================\n"
printf "  %d passed, %d failed, %d skipped\n" "$PASS" "$FAIL" "$SKIP"
printf "=========================================\n"

[ "$FAIL" -eq 0 ]
