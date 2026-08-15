#include "macos_vcan.h"
#include <stdio.h>
int main(void)
{
    struct macos_vcan *bus = macos_vcan_open("vcan0");
    struct macos_canfd_frame frame;
    unsigned int i;
    if (!bus) { perror("macos_vcan_open"); return 69; }
    for (;;) {
        if (macos_vcan_recv(bus, &frame)) { perror("macos_vcan_recv"); macos_vcan_close(bus); return 74; }
        printf("%03X [%u]", frame.can_id, frame.len);
        for (i = 0; i < frame.len; i++) printf(" %02X", frame.data[i]);
        putchar('\n'); fflush(stdout);
    }
}
