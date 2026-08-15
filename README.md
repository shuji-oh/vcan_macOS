# macos-vcan

`macos-vcan` is a local virtual CAN bus for macOS.  It provides a named bus
(`vcan0`) and a C API with Linux SocketCAN-shaped CAN/CAN-FD frame types.  It
is designed for local development, simulation, and CI.

It is deliberately **not** a kernel extension and does not create an
`ifconfig` interface.  The public macOS SDK has no supported way to add a new
BSD socket domain (`PF_CAN`) or CAN network-interface type; see
[DESIGN.md](DESIGN.md).

## Included behavior

* `vcan0` local bus served by `macos-vcand`.
* Broadcast loopback to every registered endpoint, including the sender.
* CAN 2.0 (0--8 byte payload) and CAN FD (0--64 byte payload) frames.
* Standard/extended identifiers, including the usual SocketCAN flag values.
* Per-client CAN-ID/mask receive filters.
* Local-only UNIX-domain datagram transport.

## Build and test

Xcode Command Line Tools on macOS 13 or later are required.

```sh
cd projects/macos-vcan
make
make test
```

The test uses a temporary directory below `/tmp`; it needs neither `sudo` nor
any system change.

## Try it

```sh
mkdir -p /tmp/macos-vcan
./build/macos-vcand --socket-dir /tmp/macos-vcan

# In another two terminals:
MACOS_VCAN_DIR=/tmp/macos-vcan ./build/vcan-dump
MACOS_VCAN_DIR=/tmp/macos-vcan ./build/vcan-send 123 DEADBEEF
```

`vcan-dump` blocks until a frame arrives.  CAN IDs are hexadecimal; the send
tool accepts 0--64 payload bytes as even-length hexadecimal.

## Install as a boot service

Build it locally, then install the executable and supplied LaunchDaemon plist.
The daemon uses root only to own its runtime directory; client processes can
connect to the public `vcan0` socket.

```sh
cd projects/macos-vcan
make
sudo install -d -o root -g wheel -m 0755 /usr/local/libexec/macos-vcan
sudo install -o root -g wheel -m 0755 build/macos-vcand /usr/local/libexec/macos-vcan/macos-vcand
sudo install -o root -g wheel -m 0644 com.example.macos-vcan.plist /Library/LaunchDaemons/com.example.macos-vcan.plist
sudo launchctl bootstrap system /Library/LaunchDaemons/com.example.macos-vcan.plist
sudo launchctl kickstart -k system/com.example.macos-vcan
```

The service creates `/var/run/macos-vcan/vcan0.sock`, which is the default
client location.  Confirm it with:

```sh
sudo launchctl print system/com.example.macos-vcan
ls -l /var/run/macos-vcan/vcan0.sock
```

To remove it:

```sh
sudo launchctl bootout system/com.example.macos-vcan
sudo rm /Library/LaunchDaemons/com.example.macos-vcan.plist
sudo rm /usr/local/libexec/macos-vcan/macos-vcand
```

The socket and directory are runtime state and are recreated on boot.

## C API

```c
#include "macos_vcan.h"

struct macos_vcan *bus = macos_vcan_open("vcan0");
struct macos_canfd_frame frame = { .can_id = 0x123, .len = 2, .data = {0xDE, 0xAD} };
macos_vcan_send(bus, &frame);
macos_vcan_recv(bus, &frame); /* receives own frame by default */
macos_vcan_close(bus);
```

Include `include/` and link `build/libmacos_vcan.a`.

## Scope

Programs calling Linux `socket(PF_CAN, SOCK_RAW, CAN_RAW)` need an adaptation
layer; this project does not impersonate that ABI.  It also omits CAN error
frames, BCM/ISO-TP/J1939 sockets, timestamp ancillary data, netlink, and real
CAN hardware.
