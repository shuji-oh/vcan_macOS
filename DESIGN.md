# Design and macOS boundary

Linux SocketCAN splits a virtual CAN netdevice (`vcan`) from `AF_CAN` and
`CAN_RAW`.  This project mirrors the useful runtime semantics in one service:

```
application -- libmacos_vcan -- AF_UNIX/SOCK_DGRAM -- macos-vcand -- vcan0 bus
```

The daemon owns one socket per named bus and fans valid frames out to registered
UNIX-domain endpoints.  Clients apply receive filters locally.  The transport
never leaves the host.

## Why this is not a KEXT

Current XNU source contains `net_add_domain_old()` and related
protocol-domain code, but labels the registration route **private**.  It is not
part of the public KEXT SDK and cannot safely be linked by a distributable
third-party KEXT.  The documented Network Kernel Extension API exposes socket,
IP, interface, and protocol *filters*, not creation of an arbitrary `PF_CAN`
domain.

Apple recommends System Extensions and DriverKit instead of KEXTs on current
macOS.  DriverKit serves supported hardware categories and also provides no
general BSD socket-domain registration API.  A true
`socket(PF_CAN, SOCK_RAW, CAN_RAW)` implementation would therefore require a
custom XNU build or unsupported private interfaces; this project intentionally
does neither.

For physical CAN hardware, use an appropriately supported driver path and
bridge frames in user space.  Do not use this virtual service to control
safety-critical equipment without a separately reviewed transport, access
policy, and test plan.
