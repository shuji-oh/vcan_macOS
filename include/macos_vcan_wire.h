#ifndef MACOS_VCAN_WIRE_H
#define MACOS_VCAN_WIRE_H

#include <stdint.h>
#include "macos_vcan.h"

#define MACOS_VCAN_WIRE_MAGIC 0x4d56434eU /* MVCN */
#define MACOS_VCAN_WIRE_VERSION 1U

enum macos_vcan_wire_op {
    MACOS_VCAN_HELLO = 1,
    MACOS_VCAN_FRAME = 2,
    MACOS_VCAN_GOODBYE = 3,
};

struct macos_vcan_wire {
    uint32_t magic;
    uint16_t version;
    uint16_t op;
    struct macos_canfd_frame frame;
};

#endif
