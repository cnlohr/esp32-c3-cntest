# cnip, or featherIP

## TODO

1) Make an "unsafe" mode where bounds checking is not performed in the HAL layer.
2) Make no-op pop2's just advance the pointer.
3) Make sure DHCP names are recriprocal, i.e. if server names us, accept it.
4) Make a version of the stack, or fully make the stack static, with no dynamic memory.
5) With HTTP, return the payload in the packet with the respone header *and* payload.
6) Avoid pop and push operations, in favor of operating directly on the buffers.

## Neat things I learned

LwIP's checksum code is slow.

For 1024-byte packets, generating ICMP responses.

 * Naieve implementation: 5257 cycles. 
 * LwIP v1: 7341 cycles.
 * LwIP v2: 5264 cycles.
 * cnip implementation: 3438 cycles.

## Switching from "POP" to structures...


By switching the first part of the packet arrival to structures, saved ~28 bytes IRAM.  Not much, but this is also an optimization.  Verified no downsides.

## General Bummers

1) The Ethernet stack requires a copy whenever TXing data.
