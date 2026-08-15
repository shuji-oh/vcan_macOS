#ifndef MACOS_VCAN_H
#define MACOS_VCAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SocketCAN-compatible identifier flag values. */
#define MACOS_CAN_EFF_FLAG 0x80000000U
#define MACOS_CAN_RTR_FLAG 0x40000000U
#define MACOS_CAN_ERR_FLAG 0x20000000U
#define MACOS_CAN_SFF_MASK 0x000007FFU
#define MACOS_CAN_EFF_MASK 0x1FFFFFFFU

struct macos_canfd_frame {
    uint32_t can_id;
    uint8_t len;                 /* 0..64; values 0..8 are classical CAN */
    uint8_t flags;               /* reserved for CAN FD flags */
    uint8_t __res0;
    uint8_t __res1;
    uint8_t data[64];
};

struct macos_can_filter {
    uint32_t can_id;
    uint32_t can_mask;
};

struct macos_vcan;

struct macos_vcan *macos_vcan_open(const char *ifname);
void macos_vcan_close(struct macos_vcan *bus);
int macos_vcan_get_fd(const struct macos_vcan *bus);
int macos_vcan_send(struct macos_vcan *bus, const struct macos_canfd_frame *frame);
int macos_vcan_recv(struct macos_vcan *bus, struct macos_canfd_frame *frame);
int macos_vcan_set_filters(struct macos_vcan *bus,
    const struct macos_can_filter *filters, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif
