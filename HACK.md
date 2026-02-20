# RC Protocol Reverse Engineering Guide

This document describes techniques for reverse-engineering 2.4GHz RC protocols using NRF24L01/XN297.

## Tools Required

| Tool | Purpose |
|------|---------|
| **This sniffer** | Capture and decode packets |
| **SDR** (RTL-SDR, HackRF) | Visualize RF spectrum, find active channels |
| **Logic Analyzer** | Capture SPI between original TX MCU and RF chip |
| **Original TX/RX pair** | Generate known traffic for analysis |
| **Oscilloscope** (optional) | Analyze RF signal timing |

## Step 1: Initial RF Discovery

### Find Active Channels

**Method A: Use Auto mode (recommended)**
```
> mode 3
> start
```
Auto mode scans all channels (0-84) at all bitrates (250K/1M/2M) and reports when packets are found.

**Method B: SDR Spectrum Analysis**

Use SDR software (GQRX, SDR#) to visualize the 2.4GHz band:
```
2400 MHz                                    2483.5 MHz
│                                                    │
├────────────────────────────────────────────────────┤
     ↑         ↑              ↑         ↑
   Ch 10     Ch 30          Ch 60     Ch 80
   
Look for periodic spikes indicating TX activity
```

**Method C: Manual Channel Scan**
```
> mode 1          # Try 1Mbps first (most common)
> ch 0
> start
# No packets? Try next channel
> ch 10
> restart
# Repeat...
```

### Determine Bitrate

Most RC toys use one of three bitrates:

| Bitrate | Common Use |
|---------|------------|
| 250 Kbps | Longer range toys (XK, Eachine) |
| 1 Mbps | Most common (Syma, JJRC, MJX) |
| 2 Mbps | Higher-end, lower latency |

If Auto mode doesn't find packets, try each manually:
```
> mode 0    # 250K
> mode 1    # 1M  
> mode 2    # 2M
```

## Step 2: Capture Bind Packets

Bind sequence reveals critical information:
- Bind channel (usually fixed)
- Address exchange mechanism
- Protocol handshake

**Procedure:**
1. Power off TX and model
2. Start sniffer in scan mode: `mode 3`, `start`
3. Put model in bind mode (usually power on while holding button)
4. Power on TX in bind mode
5. Capture the exchange

**Example bind capture (XK2 protocol):**
```
# Model sends bind request
RX: C=71 S=Y A= CC CC CC CC CC P(9)= 9C BB CC DD 38 12 10 00 19
                                     │  └─────┘  └─────┘
                                     │  Dummy ID  RX_ID
                                     └─ 0x9C = bind phase 1

# TX responds
RX: C=71 S=Y A= CC CC CC CC CC P(9)= 9D 66 4F 47 38 12 10 00 B3
                                     │  └─────┘  └─────┘
                                     │  TX_ID    RX_ID
                                     └─ 0x9D = bind phase 2

# Model acknowledges
RX: C=71 S=Y A= CC CC CC CC CC P(9)= 9B 66 4F 47 38 12 10 00 B0
                                     └─ 0x9B = bind complete
```

## Step 3: Analyze Hopping Pattern

### Identify Hopping Channels

After bind, switch to fixed channel mode on the bind channel:
```
> ch 71       # Stay on bind channel
> restart
```

Note: After bind, TX usually switches to hopping. You'll see packets periodically when TX hops back to this channel.

**Measure hop timing:**
```
RX: 19642us C=65 ...    # Packet on ch 65
RX: 19644us C=65 ...    # Next packet on ch 65 (~20ms later)
```

If packets come every ~20ms on one channel, and packet period is ~5ms:
- 20ms / 5ms = 4 channels in hop sequence
- TX visits this channel every 4th packet

### Map All Channels

Use Auto mode to discover all hopping channels:
```
Packet detected: bitrate=250K C=65 S=Y A= 66 4F 47 CC CC
...
4 RF channels identified: 65[12] 69[11] 73[12] 77[11]
                          └─ channel [packet count]
```

### Determine Hop Order

Auto mode measures timing between channels to find order:
```
Channel order:
65:     0us      # Reference channel
69:  4911us      # 4.9ms after ch 65
73:  9822us      # 9.8ms after ch 65  
77: 14733us      # 14.7ms after ch 65

Hop sequence: 65 → 69 → 73 → 77 → 65...
Packet period: ~4911us (~5ms)
```

## Step 4: Decode Packet Structure

### Control Channel Mapping

**Procedure:**
1. Set sniffer to fixed channel in hop sequence
2. Center all sticks, note "idle" packet
3. Move ONE control at a time, observe changes

**Example analysis:**
```
Idle (all centered):
P(9)= 32 32 00 32 60 00 01 5A 50
      │  │  │  │  
      │  │  │  └─ Rudder: 0x32 = 50 (centered)
      │  │  └──── Throttle: 0x00 = 0 (idle)
      │  └─────── Elevator: 0x32 = 50 (centered)
      └────────── Aileron: 0x32 = 50 (centered)

Full throttle:
P(9)= 32 32 64 32 60 00 01 5A 82
            └──── Throttle: 0x64 = 100 (max)
                                    └─ Checksum changed

Aileron right:
P(9)= 64 32 00 32 60 00 01 5A 82
      └──────── Aileron: 0x64 = 100 (full right)
```

### Common Value Ranges

| Type | Range | Center | Notes |
|------|-------|--------|-------|
| Stick (signed) | 0x00-0x64 | 0x32 | 0-100, center 50 |
| Stick (unsigned) | 0x00-0xFF | 0x80 | 0-255, center 128 |
| Throttle | 0x00-0x64 | N/A | 0-100, no center |
| Switch | 0x00/0x01 | N/A | Off/On |
| Flags | Bitmask | N/A | Multiple switches in one byte |

### Flag Byte Analysis

Toggle switches one at a time:
```
Base:     P[5]= 00000000 (0x00)
Rate SW:  P[5]= 00000001 (0x01)  → Bit 0 = Rate
Flip SW:  P[5]= 00000010 (0x02)  → Bit 1 = Flip  
Light SW: P[5]= 01000000 (0x40)  → Bit 6 = Light
```

### Checksum Identification

Common checksum algorithms:

**Simple sum:**
```c
checksum = sum(packet[0..N-1]) & 0xFF
```

**Sum with seed:**
```c
checksum = (sum(packet[0..N-1]) + SEED) & 0xFF
// XK2: SEED = TX_ID[0] - TX_ID[1] + TX_ID[2] + 0x21
```

**XOR:**
```c
checksum = packet[0] ^ packet[1] ^ ... ^ packet[N-1]
```

**CRC-8/CRC-16:**
```c
// More complex, need to identify polynomial
// Common: CRC-8 (0x07, 0x31), CRC-16-CCITT (0x1021)
```

**Finding the algorithm:**
1. Collect multiple packets with known differences
2. Try common algorithms
3. Look for patterns in how checksum changes

## Step 5: XN297 Scrambling

### What is Scrambling?

XN297 chips XOR data with a fixed pseudo-random sequence before transmission:

```
Original:    [Address][Payload][CRC]
                 │         │      │
                 ▼         ▼      ▼
              XOR with scramble table
                 │         │      │
                 ▼         ▼      ▼
Transmitted: [Scrambled Address][Scrambled Payload][Modified CRC]
```

### The Scramble Table

**This table is universal for ALL XN297 chips:**

```c
const uint8_t xn297_scramble[] = {
    0xE3, 0xB1, 0x4B, 0xEA, 0x85, 0xBC, 0xE5, 0x66,  // Address bytes 0-7
    0x0D, 0xAE, 0x8C, 0x88, 0x12, 0x69, 0xEE, 0x1F,  // Payload bytes 0-7
    0xC7, 0x62, 0x97, 0xD5, 0x0B, 0x79, 0xCA, 0xCC,  // Payload bytes 8-15
    0x1B, 0x5D, 0x19, 0x10, 0x24, 0xD3, 0xDC, 0x3F,  // Payload bytes 16-23
    0x8E, 0xC5, 0x2F, 0xAA, 0x16, 0xF3, 0x95        // Payload bytes 24-30
};
```

### How Was It Found?

The scramble table was discovered by **analyzing the XN297 chip**, not by reverse-engineering protocols:

1. **Known plaintext attack**: Send known data through XN297
2. **Capture with NRF24L01**: Receive the scrambled transmission
3. **XOR to extract table**: `scramble = received XOR original`
4. **Verify**: Confirm table is consistent across packets/devices

### Detecting Scrambled vs Unscrambled

The sniffer automatically detects both:
```
S=Y  → Scrambled (XN297 with scrambling enabled)
S=N  → Unscrambled (NRF24L01 or XN297 unscrambled mode)
```

### CRC XOR-out Tables

XN297 also modifies CRC calculation. The `xn297_crc_xorout` tables compensate for this:
- `xn297_crc_xorout[]` - Unscrambled packets
- `xn297_crc_xorout_scrambled[]` - Scrambled packets
- `xn297_crc_xorout_enhanced[]` - Enhanced ShockBurst mode

## Step 6: Document Your Findings

### Protocol Template

```
Protocol: [Name]
Chip: NRF24L01 / XN297
Bitrate: 250K / 1M / 2M
Scrambled: Yes / No

Bind
----
Channel: [XX]
Address: [XX XX XX XX XX]
Packet format:
  P[0] = Bind phase (0x9C=request, 0x9D=response, 0x9B=ack)
  P[1-3] = TX_ID or RX_ID
  ...
  P[N] = Checksum: sum(P[0..N-1]) + 0xXX

Normal
------
Channels: [XX, XX, XX, XX] (hop sequence)
Packet period: XXXXus
Address: [derived from bind]
Packet format:
  P[0] = Aileron (0x00-0x64, center 0x32)
  P[1] = Elevator (0x00-0x64, center 0x32)
  P[2] = Throttle (0x00-0x64)
  P[3] = Rudder (0x00-0x64, center 0x32)
  P[4] = Trims
  P[5] = Flags (bit0=rate, bit1=flip, ...)
  ...
  P[N] = Checksum

Telemetry (if any)
------------------
...
```

## Common Pitfalls

### No Packets Received
- Wrong bitrate (try all three)
- Wrong address length (try 3, 4, 5)
- Interference (move away from WiFi routers)
- Bad wiring (check SPI connections)
- NRF24L01 module issue (run `detect` command)

### Packets But Bad CRC
- Scrambled/unscrambled mismatch
- Enhanced ShockBurst mode vs standard
- Custom CRC polynomial (rare)

### Inconsistent Captures
- Frequency hopping (use fixed channel or Auto mode)
- Multiple TXs nearby
- Model not bound (capture bind first)

### Can't Find Checksum Algorithm
- May include address bytes in calculation
- May have per-packet seed (PID, counter)
- May be CRC with unusual polynomial

## Resources

- **RCGroups Deviation Thread**: https://www.rcgroups.com/forums/showthread.php?t=2165676
- **DeviationTX GitHub**: https://github.com/DeviationTX/deviation
- **Multiprotocol GitHub**: https://github.com/pascallanger/DIY-Multiprotocol-TX-Module
- **nRF24L01 Datasheet**: Search for "nRF24L01+ Product Specification"
- **XN297 Info**: Limited official docs, see Deviation/Multiprotocol source code

## Credits

Protocol reverse engineering work by:
- **DeviationTX team** (Goebish, PhracturedBlue, and others)
- **Multiprotocol team** (Pascal Langer and contributors)
- **RCGroups community**

The XN297 scramble table and much of the foundational work came from the DeviationTX project.

---

# Appendix A: XN297 Tables Reference

## All XN297 Tables Explained

### 1. `xn297_scramble[]` - Data Scramble Table

```c
const uint8_t xn297_scramble[39] = {
    0xE3, 0xB1, 0x4B, 0xEA, 0x85, 0xBC, 0xE5, 0x66,  // bytes 0-7
    0x0D, 0xAE, 0x8C, 0x88, 0x12, 0x69, 0xEE, 0x1F,  // bytes 8-15
    0xC7, 0x62, 0x97, 0xD5, 0x0B, 0x79, 0xCA, 0xCC,  // bytes 16-23
    0x1B, 0x5D, 0x19, 0x10, 0x24, 0xD3, 0xDC, 0x3F,  // bytes 24-31
    0x8E, 0xC5, 0x2F, 0xAA, 0x16, 0xF3, 0x95        // bytes 32-38
};
```

**Purpose:** XOR mask for address and payload bytes

**Usage:**
```
Byte position:     0    1    2    3    4    5    6    7   ...
                   └─────── Address ───────┘└──── Payload ────
Scramble index:    0    1    2    3    4    5    6    7   ...

TX: scrambled_byte = original_byte ^ xn297_scramble[position]
RX: original_byte = scrambled_byte ^ xn297_scramble[position]
```

---

### 2. CRC XOR-out Tables

XN297 uses CRC-16-CCITT (polynomial 0x1021, init 0xB5D2), but with a twist: **the final CRC is XORed with a value that depends on packet length**.

#### Why XOR-out Tables Exist

```
Standard NRF24L01:
  CRC = CRC16(payload)

XN297:
  CRC = CRC16(address + payload) ^ xorout[length]
```

The XOR-out compensates for how XN297 includes the (possibly scrambled) address in CRC calculation.

#### Table Index Calculation

```c
index = address_length - 3 + payload_length
// address_length: 3, 4, or 5
// payload_length: 0 to 32

// Example: 5-byte address, 9-byte payload
// index = 5 - 3 + 9 = 11
```

---

### 3. `xn297_crc_xorout_scrambled[]` - Standard Mode, Scrambled

```c
const uint16_t xn297_crc_xorout_scrambled[35] = {
    0x0000, 0x3448, 0x9BA7, 0x8BBB, 0x85E1, 0x3E8C,
    0x451E, 0x18E6, 0x6B24, 0xE7AB, 0x3828, 0x814B,
    0xD461, 0xF494, 0x2503, 0x691D, 0xFE8B, 0x9BA7,
    0x8B17, 0x2920, 0x8B5F, 0x61B1, 0xD391, 0x7401,
    0x2138, 0x129F, 0xB3A0, 0x2988, 0x23CA, 0xC0CB,
    0x0C6C, 0xB329, 0xA0A1, 0x0A16, 0xA9D0
};
```

**Used when:** Scrambling ON, Standard ShockBurst mode

**Code:**
```c
if (xn297_scramble_enabled)
    crc ^= xn297_crc_xorout_scrambled[addr_len - 3 + payload_len];
```

---

### 4. `xn297_crc_xorout[]` - Standard Mode, Unscrambled

```c
const uint16_t xn297_crc_xorout[35] = {
    0x0000, 0x3D5F, 0xA6F1, 0x3A23, 0xAA16, 0x1CAF,
    0x62B2, 0xE0EB, 0x0821, 0xBE07, 0x5F1A, 0xAF15,
    0x4F0A, 0xAD24, 0x5E48, 0xED34, 0x068C, 0xF2C9,
    0x1852, 0xDF36, 0x129D, 0xB17C, 0xD5F5, 0x70D7,
    0xB798, 0x5133, 0x67DB, 0xD94E, 0x0A5B, 0xE445,
    0xE6A5, 0x26E7, 0xBDAB, 0xC379, 0x8E20
};
```

**Used when:** Scrambling OFF, Standard ShockBurst mode

---

### 5. `xn297_crc_xorout_scrambled_enhanced[]` - Enhanced Mode, Scrambled

```c
const uint16_t xn297_crc_xorout_scrambled_enhanced[35] = {
    0x0000, 0x7EBF, 0x3ECE, 0x07A4, 0xCA52, 0x343B,
    0x53F8, 0x8CD0, 0x9EAC, 0xD0C0, 0x150D, 0x5186,
    0xD251, 0xA46F, 0x8435, 0xFA2E, 0x7EBD, 0x3C7D,
    0x94E0, 0x3D5F, 0xA685, 0x4E47, 0xF045, 0xB483,
    0x7A1F, 0xDEA2, 0x9642, 0xBF4B, 0x032F, 0x01D2,
    0xDC86, 0x92A5, 0x183A, 0xB760, 0xA953
};
```

**Used when:** Scrambling ON, Enhanced ShockBurst mode

---

### 6. `xn297_crc_xorout_enhanced[]` - Enhanced Mode, Unscrambled

```c
const uint16_t xn297_crc_xorout_enhanced[35] = {
    0x0000, 0x8BE6, 0xD8EC, 0xB87A, 0x42DC, 0xAA89,
    0x83AF, 0x10E4, 0xE83E, 0x5C29, 0xAC76, 0x1C69,
    0xA4B2, 0x5961, 0xB4D3, 0x2A50, 0xCB27, 0x5128,
    0x7CDB, 0x7A14, 0xD5D2, 0x57D7, 0xE31D, 0xCE42,
    0x648D, 0xBF2D, 0x653B, 0x190C, 0x9117, 0x9A97,
    0xABFC, 0xE68E, 0x0DE7, 0x28A2, 0x1965
};
```

**Used when:** Scrambling OFF, Enhanced ShockBurst mode

---

## Standard vs Enhanced ShockBurst Mode

| Feature | Standard Mode | Enhanced Mode |
|---------|--------------|---------------|
| Packet format | Address + Payload + CRC | Address + PCF + Payload + CRC |
| PCF (Packet Control Field) | No | Yes (length, PID, no-ack) |
| Auto retransmit | No | Yes |
| Variable payload | No | Yes |
| CRC position | Byte-aligned | Bit-shifted (6 bits offset) |

**Standard mode packet:**
```
┌───────────┬───────────┬───────┐
│  Address  │  Payload  │  CRC  │
│  3-5 B    │  1-32 B   │  16b  │
└───────────┴───────────┴───────┘
```

**Enhanced mode packet:**
```
┌───────────┬───────┬───────────┬─────────────────┐
│  Address  │  PCF  │  Payload  │       CRC       │
│  3-5 B    │  9b   │  0-32 B   │  16b (shifted)  │
└───────────┴───────┴───────────┴─────────────────┘

PCF (9 bits):
  - Payload length: 6 bits (0-32)
  - PID (Packet ID): 2 bits (0-3, for duplicate detection)
  - No-ACK flag: 1 bit
```

---

## Table Selection Summary

| Table | Scrambled | Mode | Typical Use Case |
|-------|-----------|------|------------------|
| `xn297_scramble` | - | Both | XOR data bytes |
| `xn297_crc_xorout_scrambled` | Yes | Standard | **Most common** (cheap toys) |
| `xn297_crc_xorout` | No | Standard | NRF24L01 compatible mode |
| `xn297_crc_xorout_scrambled_enhanced` | Yes | Enhanced | Bidirectional protocols |
| `xn297_crc_xorout_enhanced` | No | Enhanced | Rare |

**Note:** Most RC toys use **scrambled standard mode** (`xn297_crc_xorout_scrambled`).

---

## CRC Verification Example

```c
// Verify received packet CRC (standard mode, scrambled)
bool verify_crc(uint8_t *packet, uint8_t addr_len, uint8_t payload_len) {
    uint16_t crc = 0xB5D2;  // Initial value
    
    // Process address (already unscrambled in rx_addr)
    for (int i = 0; i < addr_len; i++)
        crc16_update(&crc, rx_addr[addr_len - 1 - i]);
    
    // Process payload (still scrambled in buffer)
    for (int i = 0; i < payload_len; i++)
        crc16_update(&crc, packet[i]);
    
    // Apply XOR-out
    crc ^= xn297_crc_xorout_scrambled[addr_len - 3 + payload_len];
    
    // Compare with received CRC
    uint16_t rx_crc = (packet[payload_len] << 8) | packet[payload_len + 1];
    return (crc == rx_crc);
}
```
