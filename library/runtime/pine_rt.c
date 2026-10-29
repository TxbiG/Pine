/*
 * Pine runtime support for the standard library modules <str>, <io> and <net>.
 *
 * Pine has no foreign-function syntax yet; instead a Pine function declared with a
 * signature and no body (`pub i32 str_len(string s);`) is an *external* function that the
 * C compiler resolves at link time. This file provides those definitions.
 *
 * Build a program that uses the library:
 *
 *     pine main.pine > main.c
 *     cc main.c library/runtime/pine_rt.c -o main          (Linux / macOS)
 *     cc main.c library/runtime/pine_rt.c -o main -lws2_32  (Windows, MinGW)
 *
 * Calling convention notes (must match what the Pine C backend emits):
 *   string -> char*, []u8 -> struct { uint8_t *data; size_t length; }, bool -> _Bool.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET pine_socket;
#define PINE_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
typedef int pine_socket;
#define PINE_INVALID_SOCKET (-1)
#endif

/* Same layout as the typedef the Pine C backend emits for []u8. */
typedef struct pine_slice_u8 {
    uint8_t *data;
    size_t length;
} pine_slice_u8;

typedef struct pine_slice_i32 {
    int32_t *data;
    size_t length;
} pine_slice_i32;

/* ------------------------------------------------------------------ str */

int32_t str_len(const char *s) {
    return s ? (int32_t)strlen(s) : 0;
}

/* Byte at index i (0..255); 0 at the terminator; -1 when out of range. */
int32_t str_at(const char *s, int32_t i) {
    if (!s || i < 0) return -1;
    size_t n = strlen(s);
    if ((size_t)i > n) return -1;
    return (int32_t)(unsigned char)s[i];
}

/* Copies the bytes of s (no terminator) into dst. Returns the length, or -1 if dst is too small. */
int32_t str_copy(const char *s, pine_slice_u8 dst) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n > dst.length) return -1;
    memcpy(dst.data, s, n);
    return (int32_t)n;
}

/* ------------------------------------------------------------------- io */

#define PINE_MAX_FILES 64
static FILE *g_files[PINE_MAX_FILES];

static FILE *io_file(int32_t h) {
    if (h == 0) return stdin;
    if (h == 1) return stdout;
    if (h == 2) return stderr;
    if (h < 3 || h >= PINE_MAX_FILES) return NULL;
    return g_files[h];
}

/* mode: 0 = read, 1 = write (truncate), 2 = append. Returns a handle >= 3, or -1. */
int32_t io_open(const char *path, int32_t mode) {
    const char *m = mode == 0 ? "rb" : mode == 1 ? "wb" : mode == 2 ? "ab" : NULL;
    if (!path || !m) return -1;
    for (int32_t h = 3; h < PINE_MAX_FILES; h++) {
        if (!g_files[h]) {
            FILE *f = fopen(path, m);
            if (!f) return -1;
            g_files[h] = f;
            return h;
        }
    }
    return -1;
}

int32_t io_close(int32_t h) {
    if (h < 3 || h >= PINE_MAX_FILES || !g_files[h]) return -1;
    int rc = fclose(g_files[h]);
    g_files[h] = NULL;
    return rc == 0 ? 0 : -1;
}

/* Reads up to buf.length bytes. Returns the count (0 at end of file) or -1 on error. */
int64_t io_read(int32_t h, pine_slice_u8 buf) {
    FILE *f = io_file(h);
    if (!f) return -1;
    size_t n = fread(buf.data, 1, buf.length, f);
    if (n == 0 && ferror(f)) return -1;
    return (int64_t)n;
}

/* Reads one line (without the trailing newline / CR) into buf.
 * Returns the line length, -1 at end of input with nothing read, -2 if buf is too small. */
int64_t io_read_line(int32_t h, pine_slice_u8 buf) {
    FILE *f = io_file(h);
    if (!f) return -1;
    size_t n = 0;
    int c;
    int any = 0;
    while ((c = fgetc(f)) != EOF) {
        any = 1;
        if (c == '\n') break;
        if (n >= buf.length) return -2;
        buf.data[n++] = (uint8_t)c;
    }
    if (!any) return -1;
    if (n > 0 && buf.data[n - 1] == '\r') n--;
    return (int64_t)n;
}

/* Writes the first n bytes of data (clamped to its length). Returns bytes written or -1. */
int64_t io_write_n(int32_t h, pine_slice_u8 data, int64_t n) {
    FILE *f = io_file(h);
    if (!f || n < 0) return -1;
    if ((uint64_t)n > data.length) n = (int64_t)data.length;
    size_t w = fwrite(data.data, 1, (size_t)n, f);
    return w == (size_t)n ? (int64_t)w : -1;
}

int64_t io_write(int32_t h, pine_slice_u8 data) {
    return io_write_n(h, data, (int64_t)data.length);
}

int32_t io_flush(int32_t h) {
    FILE *f = io_file(h);
    return f && fflush(f) == 0 ? 0 : -1;
}

void io_print(const char *s)   { if (s) fputs(s, stdout); }
void io_println(const char *s) { if (s) fputs(s, stdout); fputc('\n', stdout); }
void io_eprint(const char *s)  { if (s) fputs(s, stderr); }
void io_eprintln(const char *s){ if (s) fputs(s, stderr); fputc('\n', stderr); }
void io_print_i64(int64_t v)   { printf("%lld", (long long)v); }

bool io_file_exists(const char *path) {
    FILE *f = path ? fopen(path, "rb") : NULL;
    if (!f) return false;
    fclose(f);
    return true;
}

/* Size in bytes, or -1 if the file cannot be opened. */
int64_t io_file_size(const char *path) {
    FILE *f = path ? fopen(path, "rb") : NULL;
    if (!f) return -1;
    int64_t size = -1;
    if (fseek(f, 0, SEEK_END) == 0) size = (int64_t)ftell(f);
    fclose(f);
    return size;
}

int32_t io_remove(const char *path) {
    return path && remove(path) == 0 ? 0 : -1;
}

#ifdef _WIN32
#define PINE_EINVAL WSAEINVAL
#else
#define PINE_EINVAL EINVAL
#endif

/* ------------------------------------------------------------------ net */

#define PINE_MAX_SOCKETS 256
static pine_socket g_socks[PINE_MAX_SOCKETS];
static int g_socks_ready = 0;
static int32_t g_net_error = 0;
static char g_from_host[64];
static int32_t g_from_port = 0;

#ifdef _WIN32
typedef WSAPOLLFD pine_pollfd;
#define pine_poll_call(fds, n, ms) WSAPoll((fds), (ULONG)(n), (int)(ms))
#define pine_sock_errno() WSAGetLastError()
#define PINE_EWOULDBLOCK WSAEWOULDBLOCK
#else
typedef struct pollfd pine_pollfd;
#define pine_poll_call(fds, n, ms) poll((fds), (nfds_t)(n), (int)(ms))
#define pine_sock_errno() errno
#define PINE_EWOULDBLOCK EWOULDBLOCK
#endif

static void net_set_error(void) { g_net_error = (int32_t)pine_sock_errno(); }
static void net_fail_wouldblock(void) { g_net_error = (int32_t)PINE_EWOULDBLOCK; }

static int net_ready(void) {
    if (g_socks_ready) return 1;
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { net_set_error(); return 0; }
#endif
    for (int i = 0; i < PINE_MAX_SOCKETS; i++) g_socks[i] = PINE_INVALID_SOCKET;
    g_socks_ready = 1;
    return 1;
}

static void net_close_raw(pine_socket s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static int32_t net_store(pine_socket s) {
    for (int32_t i = 0; i < PINE_MAX_SOCKETS; i++) {
        if (g_socks[i] == PINE_INVALID_SOCKET) {
            g_socks[i] = s;
            return i;
        }
    }
    net_close_raw(s);
    g_net_error = 24; /* too many open sockets */
    return -1;
}

static pine_socket net_get(int32_t h) {
    if (!g_socks_ready || h < 0 || h >= PINE_MAX_SOCKETS) return PINE_INVALID_SOCKET;
    return g_socks[h];
}

static int net_set_nonblocking_raw(pine_socket s, int on) {
#ifdef _WIN32
    u_long mode = on ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(s, F_SETFL, flags) == 0 ? 0 : -1;
#endif
}

static int net_in_progress(int e) {
#ifdef _WIN32
    return e == WSAEWOULDBLOCK;
#else
    return e == EINPROGRESS;
#endif
}

/* Formats an address (no port) into out; stores the port when requested. */
static int net_addr_text(const struct sockaddr *sa, char *out, size_t cap, int32_t *port) {
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)sa;
        if (!inet_ntop(AF_INET, (void *)&a->sin_addr, out, cap)) return -1;
        if (port) *port = (int32_t)ntohs(a->sin_port);
        return 0;
    }
    if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)sa;
        if (!inet_ntop(AF_INET6, (void *)&a->sin6_addr, out, cap)) return -1;
        if (port) *port = (int32_t)ntohs(a->sin6_port);
        return 0;
    }
    return -1;
}

static int32_t net_copy_out(const char *text, pine_slice_u8 out) {
    size_t n = strlen(text);
    if (n > out.length) return -1;
    memcpy(out.data, text, n);
    return (int32_t)n;
}

/* ---- errors ---- */

/* OS error code (errno / WSAGetLastError) of the most recent failed call; -1 = name lookup failed. */
int32_t net_last_error(void) { return g_net_error; }

/* True when the last failure only means "no data / no room yet": a non-blocking call that
 * could not complete, or a timeout that expired. */
bool net_would_block(void) {
    int32_t e = g_net_error;
#ifdef _WIN32
    return e == WSAEWOULDBLOCK || e == WSAETIMEDOUT;
#else
    return e == EAGAIN || e == EWOULDBLOCK || e == ETIMEDOUT || e == EINPROGRESS;
#endif
}

/* Human-readable text for an error code into out (no terminator). Returns the length or -1. */
int32_t net_error_text(int32_t code, pine_slice_u8 out) {
    char tmp[256];
    if (code == -1) {
        snprintf(tmp, sizeof tmp, "host name could not be resolved");
    } else {
#ifdef _WIN32
        DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                 (DWORD)code, 0, tmp, (DWORD)sizeof tmp, NULL);
        while (n > 0 && (tmp[n - 1] == '\r' || tmp[n - 1] == '\n' || tmp[n - 1] == ' ')) tmp[--n] = '\0';
        if (n == 0) snprintf(tmp, sizeof tmp, "network error %d", (int)code);
#else
        const char *m = strerror((int)code);
        snprintf(tmp, sizeof tmp, "%s", m ? m : "unknown error");
#endif
    }
    return net_copy_out(tmp, out);
}

/* ---- TCP client ---- */

static int32_t net_connect_impl(const char *host, int32_t port, int32_t timeout_ms) {
    if (!host || port < 1 || port > 65535 || timeout_ms < 0 || !net_ready()) return -1;
    char service[16];
    snprintf(service, sizeof service, "%d", port);
    struct addrinfo hints, *res = NULL, *ai;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, service, &hints, &res) != 0) { g_net_error = -1; return -1; }
    for (ai = res; ai; ai = ai->ai_next) {
        pine_socket s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == PINE_INVALID_SOCKET) continue;
        int ok = 0;
        if (timeout_ms == 0) {
            ok = connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0;
            if (!ok) net_set_error();
        } else {
            net_set_nonblocking_raw(s, 1);
            if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0) {
                ok = 1;
            } else if (net_in_progress(pine_sock_errno())) {
                pine_pollfd p;
                p.fd = s; p.events = POLLOUT; p.revents = 0;
                int pr = pine_poll_call(&p, 1, timeout_ms);
                if (pr > 0) {
                    int err = 0;
                    socklen_t el = sizeof err;
                    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &el);
                    if (err == 0) ok = 1; else g_net_error = (int32_t)err;
                } else if (pr == 0) {
                    net_fail_wouldblock();
                } else {
                    net_set_error();
                }
            } else {
                net_set_error();
            }
            if (ok) net_set_nonblocking_raw(s, 0);
        }
        if (ok) {
            freeaddrinfo(res);
            return net_store(s);
        }
        net_close_raw(s);
    }
    freeaddrinfo(res);
    return -1;
}

/* Connects to host:port over TCP (IPv4 or IPv6). Returns a socket handle or -1. */
int32_t net_tcp_connect(const char *host, int32_t port) {
    return net_connect_impl(host, port, 0);
}

/* Like net_tcp_connect but gives up after timeout_ms (net_would_block() is then true). */
int32_t net_tcp_connect_timeout(const char *host, int32_t port, int32_t timeout_ms) {
    return net_connect_impl(host, port, timeout_ms > 0 ? timeout_ms : 0);
}

/* ---- bind helpers: TCP listeners and UDP sockets ---- */

static int32_t net_bind_impl(const char *host, int32_t port, int socktype, int family, int32_t backlog) {
    if (port < 0 || port > 65535 || !net_ready()) return -1;
    char service[16];
    snprintf(service, sizeof service, "%d", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_flags = AI_PASSIVE;
    const char *node = (host && host[0]) ? host : NULL;
    if (getaddrinfo(node, service, &hints, &res) != 0) { g_net_error = -1; return -1; }
    pine_socket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == PINE_INVALID_SOCKET) { net_set_error(); freeaddrinfo(res); return -1; }
    if (socktype == SOCK_STREAM) {
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    }
    if (family == AF_INET6) {
        int no = 0; /* dual-stack where the OS allows it */
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&no, sizeof no);
    }
    int bad = bind(s, res->ai_addr, (int)res->ai_addrlen) != 0;
    if (!bad && socktype == SOCK_STREAM) bad = listen(s, backlog > 0 ? backlog : 16) != 0;
    if (bad) {
        net_set_error();
        net_close_raw(s);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return net_store(s);
}

/* ---- TCP server ---- */

/* IPv4 listener. host "" = all interfaces; port 0 = let the OS pick (see net_local_port). */
int32_t net_tcp_listen(const char *host, int32_t port, int32_t backlog) {
    return net_bind_impl(host, port, SOCK_STREAM, AF_INET, backlog);
}

/* IPv6 listener (dual-stack where supported). host "" = all interfaces. */
int32_t net_tcp_listen6(const char *host, int32_t port, int32_t backlog) {
    return net_bind_impl(host, port, SOCK_STREAM, AF_INET6, backlog);
}

/* Blocks until a client connects; returns the new connection handle or -1. */
int32_t net_accept(int32_t listener) {
    pine_socket s = net_get(listener);
    if (s == PINE_INVALID_SOCKET) return -1;
    pine_socket c = accept(s, NULL, NULL);
    if (c == PINE_INVALID_SOCKET) { net_set_error(); return -1; }
    return net_store(c);
}

/* Like net_accept but waits at most timeout_ms; on timeout returns -1 and net_would_block() is true. */
int32_t net_accept_timeout(int32_t listener, int32_t timeout_ms) {
    pine_socket s = net_get(listener);
    if (s == PINE_INVALID_SOCKET || timeout_ms < 0) return -1;
    pine_pollfd p;
    p.fd = s; p.events = POLLIN; p.revents = 0;
    int pr = pine_poll_call(&p, 1, timeout_ms);
    if (pr == 0) { net_fail_wouldblock(); return -1; }
    if (pr < 0) { net_set_error(); return -1; }
    return net_accept(listener);
}

/* ---- UDP ---- */

/* Binds a UDP socket on an IPv4 address. host "" = all interfaces; port 0 = any free port. */
int32_t net_udp_open(const char *host, int32_t port) {
    return net_bind_impl(host, port, SOCK_DGRAM, AF_INET, 0);
}

/* Binds a UDP socket on IPv6 (dual-stack where supported). */
int32_t net_udp_open6(const char *host, int32_t port) {
    return net_bind_impl(host, port, SOCK_DGRAM, AF_INET6, 0);
}

/* UDP socket "connected" to host:port: use net_send / net_recv with it, and only that peer is heard. */
int32_t net_udp_connect(const char *host, int32_t port) {
    if (!host || port < 1 || port > 65535 || !net_ready()) return -1;
    char service[16];
    snprintf(service, sizeof service, "%d", port);
    struct addrinfo hints, *res = NULL, *ai;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, service, &hints, &res) != 0) { g_net_error = -1; return -1; }
    for (ai = res; ai; ai = ai->ai_next) {
        pine_socket s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == PINE_INVALID_SOCKET) continue;
        if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return net_store(s);
        }
        net_set_error();
        net_close_raw(s);
    }
    freeaddrinfo(res);
    return -1;
}

/* Sends the first n bytes of data (clamped) as one datagram to host:port.
 * Returns bytes sent or -1. Datagrams larger than the network allows fail. */
int64_t net_send_to(int32_t h, const char *host, int32_t port, pine_slice_u8 data, int64_t n) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET || !host || port < 1 || port > 65535 || n < 0) return -1;
    if ((uint64_t)n > data.length) n = (int64_t)data.length;
    struct sockaddr_storage local;
    socklen_t ll = sizeof local;
    int family = AF_UNSPEC;
    if (getsockname(s, (struct sockaddr *)&local, &ll) == 0) family = local.ss_family;
    char service[16];
    snprintf(service, sizeof service, "%d", port);
    struct addrinfo hints, *res = NULL, *ai;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = SOCK_DGRAM;
#ifdef AI_V4MAPPED
    if (family == AF_INET6) hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
#endif
    if (getaddrinfo(host, service, &hints, &res) != 0) { g_net_error = -1; return -1; }
    int64_t result = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        int w = (int)sendto(s, (const char *)data.data, (int)n, 0, ai->ai_addr, (int)ai->ai_addrlen);
        if (w >= 0) { result = w; break; }
        net_set_error();
    }
    freeaddrinfo(res);
    return result;
}

/* Receives one datagram (up to buf.length bytes; extra bytes are dropped on POSIX, an error on
 * Windows). Returns its size or -1. The sender is then available via net_from_host / net_from_port. */
int64_t net_recv_from(int32_t h, pine_slice_u8 buf) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    struct sockaddr_storage ss;
    socklen_t sl = sizeof ss;
    int chunk = buf.length > 1048576 ? 1048576 : (int)buf.length;
    int r = (int)recvfrom(s, (char *)buf.data, chunk, 0, (struct sockaddr *)&ss, &sl);
    if (r < 0) { net_set_error(); return -1; }
    g_from_host[0] = '\0';
    g_from_port = 0;
    net_addr_text((struct sockaddr *)&ss, g_from_host, sizeof g_from_host, &g_from_port);
    return r;
}

/* Sender of the datagram returned by the last net_recv_from. */
int32_t net_from_host(pine_slice_u8 out) { return net_copy_out(g_from_host, out); }
int32_t net_from_port(void) { return g_from_port; }

/* IPv4 multicast / broadcast. */
int32_t net_multicast_join(int32_t h, const char *group) {
    pine_socket s = net_get(h);
    struct ip_mreq m;
    if (s == PINE_INVALID_SOCKET || !group) return -1;
    memset(&m, 0, sizeof m);
    if (inet_pton(AF_INET, group, (void *)&m.imr_multiaddr) != 1) { g_net_error = PINE_EINVAL; return -1; }
    m.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&m, sizeof m) != 0) { net_set_error(); return -1; }
    return 0;
}

int32_t net_multicast_leave(int32_t h, const char *group) {
    pine_socket s = net_get(h);
    struct ip_mreq m;
    if (s == PINE_INVALID_SOCKET || !group) return -1;
    memset(&m, 0, sizeof m);
    if (inet_pton(AF_INET, group, (void *)&m.imr_multiaddr) != 1) { g_net_error = PINE_EINVAL; return -1; }
    m.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&m, sizeof m) != 0) { net_set_error(); return -1; }
    return 0;
}

/* ---- data (TCP and connected UDP) ---- */

/* Sends the first n bytes of data (clamped), looping until done. Returns n or -1. */
int64_t net_send(int32_t h, pine_slice_u8 data, int64_t n) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET || n < 0) return -1;
    if ((uint64_t)n > data.length) n = (int64_t)data.length;
    int64_t sent = 0;
    while (sent < n) {
        int chunk = (n - sent) > 1048576 ? 1048576 : (int)(n - sent);
        int w = (int)send(s, (const char *)data.data + sent, chunk, 0);
        if (w <= 0) { net_set_error(); return -1; }
        sent += w;
    }
    return sent;
}

/* Sends a NUL-terminated string (without the terminator). Returns bytes sent or -1. */
int64_t net_send_str(int32_t h, const char *text) {
    if (!text) return -1;
    pine_slice_u8 d = { (uint8_t *)text, strlen(text) };
    return net_send(h, d, (int64_t)d.length);
}

/* Receives up to buf.length bytes. Returns the count, 0 if the peer closed, -1 on error/timeout. */
int64_t net_recv(int32_t h, pine_slice_u8 buf) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    int chunk = buf.length > 1048576 ? 1048576 : (int)buf.length;
    int r = (int)recv(s, (char *)buf.data, chunk, 0);
    if (r < 0) { net_set_error(); return -1; }
    return r;
}

/* Like net_recv but leaves the data queued for the next read. */
int64_t net_peek(int32_t h, pine_slice_u8 buf) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    int chunk = buf.length > 1048576 ? 1048576 : (int)buf.length;
    int r = (int)recv(s, (char *)buf.data, chunk, MSG_PEEK);
    if (r < 0) { net_set_error(); return -1; }
    return r;
}

/* Reads until exactly n bytes (clamped to buf.length) arrive or the peer closes.
 * Returns the bytes read (less than n only if the peer closed) or -1 on error. */
int64_t net_recv_exact(int32_t h, pine_slice_u8 buf, int64_t n) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET || n < 0) return -1;
    if ((uint64_t)n > buf.length) n = (int64_t)buf.length;
    int64_t got = 0;
    while (got < n) {
        int chunk = (n - got) > 1048576 ? 1048576 : (int)(n - got);
        int r = (int)recv(s, (char *)buf.data + got, chunk, 0);
        if (r < 0) { net_set_error(); return -1; }
        if (r == 0) break;
        got += r;
    }
    return got;
}

/* how: 0 = stop receiving, 1 = stop sending (peer sees end of stream), 2 = both. */
int32_t net_shutdown(int32_t h, int32_t how) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET || how < 0 || how > 2) return -1;
    if (shutdown(s, how) != 0) { net_set_error(); return -1; }
    return 0;
}

int32_t net_close(int32_t h) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    net_close_raw(s);
    g_socks[h] = PINE_INVALID_SOCKET;
    return 0;
}

/* ---- readiness ---- */

/* events/revents bits: 1 = readable, 2 = writable, 4 = error, 8 = hang-up, 16 = invalid handle. */
static short net_events_to_native(int32_t e) {
    short r = 0;
    if (e & 1) r |= POLLIN;
    if (e & 2) r |= POLLOUT;
    return r;
}

static int32_t net_events_from_native(short r) {
    int32_t e = 0;
    if (r & POLLIN) e |= 1;
    if (r & POLLOUT) e |= 2;
    if (r & POLLERR) e |= 4;
    if (r & POLLHUP) e |= 8;
    if (r & POLLNVAL) e |= 16;
    return e;
}

/* Waits up to timeout_ms (negative = forever, 0 = just check) for the requested events on one
 * socket. Returns the ready bits (0 on timeout) or -1 on error. */
int32_t net_poll(int32_t h, int32_t events, int32_t timeout_ms) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    pine_pollfd p;
    p.fd = s; p.events = net_events_to_native(events); p.revents = 0;
    int pr = pine_poll_call(&p, 1, timeout_ms);
    if (pr < 0) { net_set_error(); return -1; }
    return pr == 0 ? 0 : net_events_from_native(p.revents);
}

/* Polls up to 64 sockets at once. socks[i] / events[i] go in, revents[i] comes out (same bits as
 * net_poll). Returns how many sockets are ready (0 on timeout) or -1 on error. */
int32_t net_poll_many(pine_slice_i32 socks, pine_slice_i32 events, pine_slice_i32 revents,
                      int32_t count, int32_t timeout_ms) {
    if (count < 0 || count > 64 || (size_t)count > socks.length ||
        (size_t)count > events.length || (size_t)count > revents.length) return -1;
    pine_pollfd fds[64];
    for (int32_t i = 0; i < count; i++) {
        pine_socket s = net_get(socks.data[i]);
        fds[i].fd = s;
        fds[i].events = net_events_to_native(events.data[i]);
        fds[i].revents = 0;
    }
    int pr = count == 0 ? 0 : pine_poll_call(fds, count, timeout_ms);
    if (pr < 0) { net_set_error(); return -1; }
    int32_t ready = 0;
    for (int32_t i = 0; i < count; i++) {
        revents.data[i] = net_get(socks.data[i]) == PINE_INVALID_SOCKET ? 16 : net_events_from_native(fds[i].revents);
        if (revents.data[i] != 0) ready++;
    }
    return ready;
}

/* ---- socket options (all return 0, or -1 on failure) ---- */

static int32_t net_setopt_int(int32_t h, int level, int opt, int value) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    if (setsockopt(s, level, opt, (const char *)&value, sizeof value) != 0) { net_set_error(); return -1; }
    return 0;
}

int32_t net_set_nonblocking(int32_t h, bool on) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET) return -1;
    if (net_set_nonblocking_raw(s, on ? 1 : 0) != 0) { net_set_error(); return -1; }
    return 0;
}

int32_t net_set_nodelay(int32_t h, bool on)   { return net_setopt_int(h, IPPROTO_TCP, TCP_NODELAY, on ? 1 : 0); }
int32_t net_set_keepalive(int32_t h, bool on) { return net_setopt_int(h, SOL_SOCKET, SO_KEEPALIVE, on ? 1 : 0); }
int32_t net_set_reuseaddr(int32_t h, bool on) { return net_setopt_int(h, SOL_SOCKET, SO_REUSEADDR, on ? 1 : 0); }
int32_t net_set_broadcast(int32_t h, bool on) { return net_setopt_int(h, SOL_SOCKET, SO_BROADCAST, on ? 1 : 0); }
int32_t net_set_recv_buffer(int32_t h, int32_t bytes) { return bytes > 0 ? net_setopt_int(h, SOL_SOCKET, SO_RCVBUF, bytes) : -1; }
int32_t net_set_send_buffer(int32_t h, int32_t bytes) { return bytes > 0 ? net_setopt_int(h, SOL_SOCKET, SO_SNDBUF, bytes) : -1; }
int32_t net_set_multicast_ttl(int32_t h, int32_t ttl)  { return (ttl >= 0 && ttl <= 255) ? net_setopt_int(h, IPPROTO_IP, IP_MULTICAST_TTL, ttl) : -1; }
int32_t net_set_multicast_loop(int32_t h, bool on)     { return net_setopt_int(h, IPPROTO_IP, IP_MULTICAST_LOOP, on ? 1 : 0); }

/* With on = true, close() on a TCP socket blocks up to `seconds` for unsent data. */
int32_t net_set_linger(int32_t h, bool on, int32_t seconds) {
    pine_socket s = net_get(h);
    struct linger l;
    if (s == PINE_INVALID_SOCKET || seconds < 0) return -1;
    l.l_onoff = on ? 1 : 0;
    l.l_linger = (unsigned short)seconds;
    if (setsockopt(s, SOL_SOCKET, SO_LINGER, (const char *)&l, sizeof l) != 0) { net_set_error(); return -1; }
    return 0;
}

static int32_t net_set_timeout_opt(int32_t h, int opt, int32_t ms) {
    pine_socket s = net_get(h);
    if (s == PINE_INVALID_SOCKET || ms < 0) return -1;
#ifdef _WIN32
    DWORD t = (DWORD)ms;
    int rc = setsockopt(s, SOL_SOCKET, opt, (const char *)&t, sizeof t);
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    int rc = setsockopt(s, SOL_SOCKET, opt, &tv, sizeof tv);
#endif
    if (rc != 0) { net_set_error(); return -1; }
    return 0;
}

int32_t net_set_recv_timeout_ms(int32_t h, int32_t ms) { return net_set_timeout_opt(h, SO_RCVTIMEO, ms); }
int32_t net_set_send_timeout_ms(int32_t h, int32_t ms) { return net_set_timeout_opt(h, SO_SNDTIMEO, ms); }

/* Receive and send timeout in milliseconds; 0 disables it. */
int32_t net_set_timeout_ms(int32_t h, int32_t ms) {
    int32_t a = net_set_timeout_opt(h, SO_RCVTIMEO, ms);
    int32_t b = net_set_timeout_opt(h, SO_SNDTIMEO, ms);
    return (a == 0 && b == 0) ? 0 : -1;
}

/* ---- addresses and names ---- */

/* Port a socket is bound to, or -1. */
int32_t net_local_port(int32_t h) {
    pine_socket s = net_get(h);
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    char text[64];
    int32_t port = -1;
    if (s == PINE_INVALID_SOCKET) return -1;
    if (getsockname(s, (struct sockaddr *)&ss, &len) != 0) { net_set_error(); return -1; }
    if (net_addr_text((struct sockaddr *)&ss, text, sizeof text, &port) != 0) return -1;
    return port;
}

/* Local address of a socket as text (no port) into out. Returns the length or -1. */
int32_t net_local_addr(int32_t h, pine_slice_u8 out) {
    pine_socket s = net_get(h);
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    char text[64];
    if (s == PINE_INVALID_SOCKET) return -1;
    if (getsockname(s, (struct sockaddr *)&ss, &len) != 0) { net_set_error(); return -1; }
    if (net_addr_text((struct sockaddr *)&ss, text, sizeof text, NULL) != 0) return -1;
    return net_copy_out(text, out);
}

/* Address / port of the peer a socket is connected to. */
int32_t net_remote_addr(int32_t h, pine_slice_u8 out) {
    pine_socket s = net_get(h);
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    char text[64];
    if (s == PINE_INVALID_SOCKET) return -1;
    if (getpeername(s, (struct sockaddr *)&ss, &len) != 0) { net_set_error(); return -1; }
    if (net_addr_text((struct sockaddr *)&ss, text, sizeof text, NULL) != 0) return -1;
    return net_copy_out(text, out);
}

int32_t net_remote_port(int32_t h) {
    pine_socket s = net_get(h);
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    char text[64];
    int32_t port = -1;
    if (s == PINE_INVALID_SOCKET) return -1;
    if (getpeername(s, (struct sockaddr *)&ss, &len) != 0) { net_set_error(); return -1; }
    if (net_addr_text((struct sockaddr *)&ss, text, sizeof text, &port) != 0) return -1;
    return port;
}

/* DNS: first address for `host` as text into out. Returns the length or -1. */
int32_t net_resolve(const char *host, pine_slice_u8 out) {
    struct addrinfo hints, *res = NULL;
    char text[64];
    if (!host || !net_ready()) return -1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) { g_net_error = -1; return -1; }
    int rc = net_addr_text(res->ai_addr, text, sizeof text, NULL);
    freeaddrinfo(res);
    return rc == 0 ? net_copy_out(text, out) : -1;
}

/* This machine's host name into out. Returns the length or -1. */
int32_t net_hostname(pine_slice_u8 out) {
    char name[256];
    if (!net_ready()) return -1;
    if (gethostname(name, sizeof name) != 0) { net_set_error(); return -1; }
    name[sizeof name - 1] = '\0';
    return net_copy_out(name, out);
}

bool net_is_ipv4(const char *text) {
    struct in_addr a;
    return text && inet_pton(AF_INET, text, (void *)&a) == 1;
}

bool net_is_ipv6(const char *text) {
    struct in6_addr a;
    return text && inet_pton(AF_INET6, text, (void *)&a) == 1;
}

/* ---- byte order: pack/unpack integers in buffers (return 0 / the value, or -1 if out of range) ---- */

static int net_in_range(pine_slice_u8 b, int32_t off, int width) {
    return off >= 0 && (size_t)off + (size_t)width <= b.length;
}

int32_t net_put_u16(pine_slice_u8 b, int32_t off, int32_t v) {      /* big endian (network order) */
    if (!net_in_range(b, off, 2) || v < 0 || v > 65535) return -1;
    b.data[off] = (uint8_t)(v >> 8); b.data[off + 1] = (uint8_t)v;
    return 0;
}
int32_t net_put_u16_le(pine_slice_u8 b, int32_t off, int32_t v) {
    if (!net_in_range(b, off, 2) || v < 0 || v > 65535) return -1;
    b.data[off] = (uint8_t)v; b.data[off + 1] = (uint8_t)(v >> 8);
    return 0;
}
int32_t net_put_u32(pine_slice_u8 b, int32_t off, int64_t v) {
    if (!net_in_range(b, off, 4) || v < 0 || v > 4294967295LL) return -1;
    for (int i = 0; i < 4; i++) b.data[off + i] = (uint8_t)(v >> (8 * (3 - i)));
    return 0;
}
int32_t net_put_u32_le(pine_slice_u8 b, int32_t off, int64_t v) {
    if (!net_in_range(b, off, 4) || v < 0 || v > 4294967295LL) return -1;
    for (int i = 0; i < 4; i++) b.data[off + i] = (uint8_t)(v >> (8 * i));
    return 0;
}
int32_t net_get_u16(pine_slice_u8 b, int32_t off) {
    if (!net_in_range(b, off, 2)) return -1;
    return (int32_t)((b.data[off] << 8) | b.data[off + 1]);
}
int32_t net_get_u16_le(pine_slice_u8 b, int32_t off) {
    if (!net_in_range(b, off, 2)) return -1;
    return (int32_t)((b.data[off + 1] << 8) | b.data[off]);
}
int64_t net_get_u32(pine_slice_u8 b, int32_t off) {
    if (!net_in_range(b, off, 4)) return -1;
    return ((int64_t)b.data[off] << 24) | ((int64_t)b.data[off + 1] << 16) | ((int64_t)b.data[off + 2] << 8) | b.data[off + 3];
}
int64_t net_get_u32_le(pine_slice_u8 b, int32_t off) {
    if (!net_in_range(b, off, 4)) return -1;
    return ((int64_t)b.data[off + 3] << 24) | ((int64_t)b.data[off + 2] << 16) | ((int64_t)b.data[off + 1] << 8) | b.data[off];
}
