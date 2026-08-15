#include "macos_vcan.h"
#include "macos_vcan_wire.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_FILTERS 128

struct macos_vcan {
    int fd;
    char client_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    struct sockaddr_un server;
    struct macos_can_filter filters[MAX_FILTERS];
    uint32_t filter_count;
};

static int valid_frame(const struct macos_canfd_frame *frame)
{
    uint32_t id_mask = (frame->can_id & MACOS_CAN_EFF_FLAG) ? MACOS_CAN_EFF_MASK : MACOS_CAN_SFF_MASK;
    return frame->len <= sizeof(frame->data) &&
        (frame->can_id & ~(MACOS_CAN_EFF_FLAG | MACOS_CAN_RTR_FLAG | MACOS_CAN_ERR_FLAG | id_mask)) == 0;
}

static int send_wire(struct macos_vcan *bus, uint16_t op, const struct macos_canfd_frame *frame)
{
    struct macos_vcan_wire wire = {0};
    wire.magic = MACOS_VCAN_WIRE_MAGIC;
    wire.version = MACOS_VCAN_WIRE_VERSION;
    wire.op = op;
    if (frame != NULL) wire.frame = *frame;
    return sendto(bus->fd, &wire, sizeof(wire), 0, (const struct sockaddr *)&bus->server,
        sizeof(bus->server)) == (ssize_t)sizeof(wire) ? 0 : -1;
}

struct macos_vcan *macos_vcan_open(const char *ifname)
{
    const char *dir = getenv("MACOS_VCAN_DIR");
    struct macos_vcan *bus;
    struct sockaddr_un local = {0};
    if (ifname == NULL || strcmp(ifname, "vcan0") != 0) { errno = ENODEV; return NULL; }
    if (dir == NULL || dir[0] == '\0') dir = "/var/run/macos-vcan";
    bus = calloc(1, sizeof(*bus));
    if (bus == NULL) return NULL;
    bus->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (bus->fd < 0) { free(bus); return NULL; }
    if (snprintf(bus->client_path, sizeof(bus->client_path), "/tmp/macos-vcan.%ld.%u.sock",
        (long)getpid(), arc4random()) >= (int)sizeof(bus->client_path) ||
        snprintf(bus->server.sun_path, sizeof(bus->server.sun_path), "%s/%s.sock", dir, ifname) >=
        (int)sizeof(bus->server.sun_path)) { errno = ENAMETOOLONG; macos_vcan_close(bus); return NULL; }
    local.sun_family = AF_UNIX;
    (void)strlcpy(local.sun_path, bus->client_path, sizeof(local.sun_path));
    unlink(bus->client_path);
    if (bind(bus->fd, (const struct sockaddr *)&local, sizeof(local)) != 0) { macos_vcan_close(bus); return NULL; }
    bus->server.sun_family = AF_UNIX;
    if (send_wire(bus, MACOS_VCAN_HELLO, NULL) != 0) { macos_vcan_close(bus); return NULL; }
    return bus;
}

void macos_vcan_close(struct macos_vcan *bus)
{
    if (bus == NULL) return;
    if (bus->fd >= 0) { (void)send_wire(bus, MACOS_VCAN_GOODBYE, NULL); close(bus->fd); }
    if (bus->client_path[0] != '\0') unlink(bus->client_path);
    free(bus);
}

int macos_vcan_get_fd(const struct macos_vcan *bus) { return bus == NULL ? -1 : bus->fd; }

int macos_vcan_send(struct macos_vcan *bus, const struct macos_canfd_frame *frame)
{
    if (bus == NULL || frame == NULL || !valid_frame(frame)) { errno = EINVAL; return -1; }
    return send_wire(bus, MACOS_VCAN_FRAME, frame);
}

static int matches_filters(const struct macos_vcan *bus, const struct macos_canfd_frame *frame)
{
    uint32_t i;
    if (bus->filter_count == 0) return 1;
    for (i = 0; i < bus->filter_count; i++)
        if ((frame->can_id & bus->filters[i].can_mask) ==
            (bus->filters[i].can_id & bus->filters[i].can_mask)) return 1;
    return 0;
}

int macos_vcan_recv(struct macos_vcan *bus, struct macos_canfd_frame *frame)
{
    struct macos_vcan_wire wire;
    ssize_t received;
    if (bus == NULL || frame == NULL) { errno = EINVAL; return -1; }
    for (;;) {
        received = recv(bus->fd, &wire, sizeof(wire), 0);
        if (received < 0) return -1;
        if (received == (ssize_t)sizeof(wire) && wire.magic == MACOS_VCAN_WIRE_MAGIC &&
            wire.version == MACOS_VCAN_WIRE_VERSION && wire.op == MACOS_VCAN_FRAME &&
            valid_frame(&wire.frame) && matches_filters(bus, &wire.frame)) { *frame = wire.frame; return 0; }
    }
}

int macos_vcan_set_filters(struct macos_vcan *bus, const struct macos_can_filter *filters, uint32_t count)
{
    if (bus == NULL || count > MAX_FILTERS || (count != 0 && filters == NULL)) { errno = EINVAL; return -1; }
    if (count != 0) memcpy(bus->filters, filters, count * sizeof(*filters));
    bus->filter_count = count;
    return 0;
}
