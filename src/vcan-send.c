#include "macos_vcan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_byte(const char *p, unsigned char *out)
{
    char tmp[3] = {p[0], p[1], '\0'}, *end;
    long value = strtol(tmp, &end, 16);
    if (*end != '\0' || value < 0 || value > 255) return -1;
    *out = (unsigned char)value;
    return 0;
}
int main(int argc, char **argv)
{
    struct macos_canfd_frame frame = {0};
    struct macos_vcan *bus;
    char *end;
    size_t i, hex_len;
    if (argc != 3) { fprintf(stderr, "usage: %s CAN_ID HEX_PAYLOAD\n", argv[0]); return 64; }
    frame.can_id = (uint32_t)strtoul(argv[1], &end, 16);
    if (*end != '\0') { fprintf(stderr, "invalid CAN_ID\n"); return 64; }
    hex_len = strlen(argv[2]);
    if (hex_len % 2 != 0 || hex_len / 2 > sizeof(frame.data)) { fprintf(stderr, "payload must be 0..64 bytes of hex\n"); return 64; }
    frame.len = (uint8_t)(hex_len / 2);
    for (i = 0; i < frame.len; i++) if (hex_byte(argv[2] + i * 2, &frame.data[i])) { fprintf(stderr, "invalid payload\n"); return 64; }
    bus = macos_vcan_open("vcan0");
    if (!bus) { perror("macos_vcan_open"); return 69; }
    if (macos_vcan_send(bus, &frame)) { perror("macos_vcan_send"); macos_vcan_close(bus); return 74; }
    macos_vcan_close(bus);
    return 0;
}
