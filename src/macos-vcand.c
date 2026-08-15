#include "macos_vcan_wire.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 128
#define CLIENT_TTL_SECONDS 300

struct client { struct sockaddr_un address; time_t seen; };
static volatile sig_atomic_t stopping;
static const char *socket_path;

static void stop(int signo) { (void)signo; stopping = 1; }

static int valid_frame(const struct macos_canfd_frame *frame)
{
    uint32_t id_mask = (frame->can_id & MACOS_CAN_EFF_FLAG) ? MACOS_CAN_EFF_MASK : MACOS_CAN_SFF_MASK;
    return (frame->can_id & MACOS_CAN_ERR_FLAG) == 0 && frame->len <= sizeof(frame->data) &&
        (frame->can_id & ~(MACOS_CAN_EFF_FLAG | MACOS_CAN_RTR_FLAG | MACOS_CAN_ERR_FLAG | id_mask)) == 0;
}
static int same_peer(const struct sockaddr_un *a, const struct sockaddr_un *b)
{ return strncmp(a->sun_path, b->sun_path, sizeof(a->sun_path)) == 0; }
static void forget_client(struct client clients[], size_t *count, size_t index)
{ clients[index] = clients[*count - 1]; --*count; }
static void register_client(struct client clients[], size_t *count, const struct sockaddr_un *peer, time_t now)
{
    size_t i;
    if (peer->sun_path[0] == '\0') return;
    for (i = 0; i < *count; i++) if (same_peer(&clients[i].address, peer)) { clients[i].seen = now; return; }
    if (*count < MAX_CLIENTS) { clients[*count].address = *peer; clients[*count].seen = now; ++*count; }
}
static void broadcast(int fd, struct client clients[], size_t *count, const struct macos_vcan_wire *wire, time_t now)
{
    size_t i = 0;
    while (i < *count) {
        if (now - clients[i].seen > CLIENT_TTL_SECONDS ||
            sendto(fd, wire, sizeof(*wire), 0, (const struct sockaddr *)&clients[i].address,
            sizeof(clients[i].address)) != (ssize_t)sizeof(*wire)) forget_client(clients, count, i);
        else ++i;
    }
}

int main(int argc, char **argv)
{
    const char *dir = "/var/run/macos-vcan";
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    struct sockaddr_un server = {0}, peer = {0};
    struct client clients[MAX_CLIENTS];
    size_t client_count = 0;
    int fd, opt = 1;
    struct sigaction action = {0};
    if (argc == 3 && strcmp(argv[1], "--socket-dir") == 0) dir = argv[2];
    else if (argc != 1) { fprintf(stderr, "usage: %s [--socket-dir directory]\n", argv[0]); return EX_USAGE; }
    if (snprintf(path, sizeof(path), "%s/vcan0.sock", dir) >= (int)sizeof(path)) {
        fprintf(stderr, "socket path is too long\n"); return EX_USAGE;
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { perror("mkdir socket directory"); return EX_CANTCREAT; }
    if (chmod(dir, 0755) != 0) { perror("chmod socket directory"); return EX_CANTCREAT; }
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return EX_OSERR; }
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
    server.sun_family = AF_UNIX;
    (void)strlcpy(server.sun_path, path, sizeof(server.sun_path));
    unlink(path);
    if (bind(fd, (const struct sockaddr *)&server, sizeof(server)) != 0) { perror("bind"); close(fd); return EX_CANTCREAT; }
    if (chmod(path, 0666) != 0) { perror("chmod socket"); unlink(path); close(fd); return EX_CANTCREAT; }
    socket_path = path;
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0; /* recvfrom(2) must return EINTR so shutdown is prompt. */
    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);
    while (!stopping) {
        struct macos_vcan_wire wire;
        socklen_t peer_len = sizeof(peer);
        ssize_t n = recvfrom(fd, &wire, sizeof(wire), 0, (struct sockaddr *)&peer, &peer_len);
        time_t now = time(NULL);
        if (n < 0) { if (errno == EINTR) continue; perror("recvfrom"); break; }
        if (n != (ssize_t)sizeof(wire) || wire.magic != MACOS_VCAN_WIRE_MAGIC || wire.version != MACOS_VCAN_WIRE_VERSION) continue;
        if (wire.op == MACOS_VCAN_HELLO) register_client(clients, &client_count, &peer, now);
        else if (wire.op == MACOS_VCAN_GOODBYE) {
            size_t i;
            for (i = 0; i < client_count; i++) if (same_peer(&clients[i].address, &peer)) { forget_client(clients, &client_count, i); break; }
        } else if (wire.op == MACOS_VCAN_FRAME && valid_frame(&wire.frame)) {
            register_client(clients, &client_count, &peer, now);
            broadcast(fd, clients, &client_count, &wire, now);
        }
    }
    close(fd); unlink(socket_path); return 0;
}
