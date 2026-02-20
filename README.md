# NRF24L01 RC Protocol Sniffer

A standalone 2.4GHz packet sniffer for reverse-engineering RC model protocols. Based on the XN297Dump from the [DIY-Multiprotocol-TX-Module](https://github.com/pascallanger/DIY-Multiprotocol-TX-Module) project.

## 1. What is This Tool For?

This tool captures and decodes 2.4GHz packets from RC (Radio Control) toys, drones, and other devices that use NRF24L01 or XN297-compatible protocols.

**Use cases:**
- Reverse-engineer unknown RC protocols
- Analyze packet structure and timing
- Identify RF channels, addresses, and payload formats
- Study frequency hopping patterns
- Develop custom transmitters/receivers

**Supported protocols:**
- NRF24L01 raw packets (250Kbps, 1Mbps, 2Mbps)
- XN297 scrambled/unscrambled packets
- XN297 Enhanced ShockBurst mode
- Auto-detection mode for unknown protocols

## 2. Hardware

### Supported Platforms

| Platform | MCU | Notes |
|----------|-----|-------|
| STM32F103 | STM32F103C8/RC | 4-in-1 Multiprotocol module compatible |
| ESP32-S3 | ESP32-S3 | DevKitC or similar |

### Wiring

**STM32F103 (default pins):**
```
STM32       NRF24L01
─────       ────────
PA5 (SCK)   SCK
PA6 (MISO)  MISO
PA7 (MOSI)  MOSI
PB7         CSN
3.3V        VCC
GND         GND
CE          VCC (tie high)
IRQ         (not used)
```

**ESP32-S3 (default pins):**
```
ESP32-S3    NRF24L01
────────    ────────
GPIO18      SCK
GPIO19      MISO
GPIO23      MOSI
GPIO5       CSN
GPIO4       CE
3.3V        VCC
GND         GND
IRQ         (not used)
```

### NRF24L01 Module

```
    ┌─────────────────┐
    │  NRF24L01+ PA   │
    │    (with LNA)   │
    │                 │
    │ GND  VCC        │
    │ CE   CSN        │
    │ SCK  MOSI       │
    │ MISO IRQ        │
    └─────────────────┘
```

> **Note:** The IRQ pin is not used. This tool uses polling instead of interrupts.

## 3. Tool Usage

### Build & Flash

```bash
# Build for STM32
pio run -e stm32f103

# Build for ESP32-S3
pio run -e esp32s3

# Upload
pio run -e stm32f103 -t upload
```

### CLI Commands

Connect via serial terminal (115200 baud). Available commands:

| Command | Description |
|---------|-------------|
| `help` | Show all commands |
| `status` | Show current settings |
| `detect` | Check if NRF24L01 is connected |
| `mode <0-6>` | Set protocol mode |
| `ch <0-84\|255\|scan>` | Set RF channel |
| `addr <3-5>` | Set address length |
| `start` | Start sniffing |
| `stop` | Stop sniffing |
| `restart` | Restart with current settings |

### Mode Parameter

| Mode | Bitrate | Description |
|------|---------|-------------|
| 0 | 250 Kbps | XN297/NRF24L01 low-speed mode |
| 1 | 1 Mbps | Standard NRF24L01 speed |
| 2 | 2 Mbps | High-speed mode |
| 3 | Auto | Auto-detect protocol and channels |
| 4 | NRF | Raw NRF24L01 mode |
| 6 | XN297 | XN297 emulation mode |

### Channel Parameter

| Value | Description |
|-------|-------------|
| 0-84 | Fixed RF channel (2400 + ch MHz) |
| 255 or `scan` | Scan all channels |

**Example: Channel to Frequency**
```
Channel 65 → 2400 + 65 = 2465 MHz
Channel 77 → 2400 + 77 = 2477 MHz
```

### Address Length Parameter

Most RC protocols use 5-byte addresses. Some use 3 or 4 bytes.

### Example Session

```
> detect
Detecting NRF24L01... FOUND

> mode 0
Mode set to 0 (250K)

> ch 65
Channel set to 65 (0x41)

> addr 5
Address length set to 5

> start
Starting dump...
Initialized: mode=0 ch=65 addr=5
XN297 dump, address length=5, bitrate=250K
RX: 19642us C=65 S=Y A= 66 4F 47 CC CC P(9)= 32 32 00 32 E0 00 01 5A 50
RX: 19644us C=65 S=Y A= 66 4F 47 CC CC P(9)= 32 32 00 32 E0 00 01 5A 51
```

**Output format:**
- `RX: 19642us` - Time since last packet (microseconds)
- `C=65` - RF channel
- `S=Y` - Scrambled (Y=XN297 scrambled, N=unscrambled)
- `A= 66 4F 47 CC CC` - 5-byte address
- `P(9)=` - Payload (9 bytes)

## 4. 2.4GHz GFSK Modulation

### What is GFSK?

**GFSK** (Gaussian Frequency Shift Keying) is a modulation technique used by NRF24L01, Bluetooth, and many other 2.4GHz devices.

**FSK vs GFSK comparison:**

```
    FSK (abrupt transitions)          GFSK (smooth Gaussian transitions)
    
    Frequency                         Frequency
        ↑                                 ↑
   f1 ──┼──┐     ┌──┐     ┌──           ──┼──╮     ╭──╮     ╭──
        │  │     │  │     │               │  ╲   ╱  ╲   ╱
   fc ──┼──┼─────┼──┼─────┼──           ──┼───╲─╱────╲─╱────
        │  │     │  │     │               │    ╳      ╳
   f0 ──┼──┘     └──┘     └──           ──┼──╱ ╲    ╱ ╲
        │                                 │ ╱   ╲──╱   ╲──
        └──────────────────→ Time         └──────────────────→ Time
              
    Binary:  1  0  1  0  1                Binary:  1  0  1  0  1
    
    Problem: Sharp transitions           Solution: Gaussian filter smooths
    create wide bandwidth                transitions, reduces bandwidth
```

**Key concepts:**
- **Bit "1"**: Frequency shifts UP from center (e.g., +160kHz for NRF24L01)
- **Bit "0"**: Frequency shifts DOWN from center (e.g., -160kHz)
- **Gaussian filter**: Smooths the frequency transitions to reduce spectral bandwidth
- **BT (Bandwidth-Time product)**: NRF24L01 uses BT=0.5, which balances bandwidth efficiency and ISI (Inter-Symbol Interference)

### 2.4GHz ISM Band Comparison

```
    2.400 GHz                                    2.4835 GHz
    │                                                    │
    ▼                                                    ▼
    ├────────────────────────────────────────────────────┤
    │                  ISM Band (83.5 MHz)               │
    │                                                    │
    │  NRF24L01/XN297 (1-2 MHz channels, 126 channels)  │
    │  ├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤   │
    │  0 5 10  20  30  40  50  60  70  80 100 125       │
    │                                                    │
    │  Bluetooth Classic (1 MHz channels, 79 channels)  │
    │  ├┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┤  │
    │  0                    39                    78     │
    │                                                    │
    │  Bluetooth LE (2 MHz channels, 40 channels)       │
    │  ├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤         │
    │  0    10         20         30       39           │
    │  (Adv: 37,38,39 spread across band)               │
    │                                                    │
    │  WiFi 2.4GHz (20/40 MHz channels)                 │
    │  ├──────┼──────┼──────┼──────┼──────┼──────┤      │
    │   Ch1    Ch3    Ch6    Ch9    Ch11   Ch13         │
    └────────────────────────────────────────────────────┘
```

### Protocol Comparison

| Feature | NRF24L01/XN297 | BT Classic | Bluetooth LE | WiFi 2.4GHz |
|---------|----------------|------------|--------------|-------------|
| **Modulation** | GFSK | GFSK | GFSK | OFDM |
| **Channel Width** | 1 MHz | 1 MHz | 2 MHz | 20/40 MHz |
| **Channels** | 126 (0-125) | 79 | 40 | 13-14 |
| **Data Rate** | 250K/1M/2M | 1M/2M/3M | 1M/2M | Up to 600M |
| **Range** | 10-100m | 10-100m | 10-100m | 30-100m |
| **TX Power** | ~12mA | ~25mA | ~15mA | ~200mA |
| **Hopping** | App-defined | 1600 hops/s | 1600 hops/s | No |
| **Packet Size** | 1-32 bytes | Up to 1021 | 0-255 bytes | Up to 2304 |
| **Latency** | <1ms | ~100ms | ~3ms | ~10ms |
| **Use Case** | RC toys, sensors | Audio, file transfer | IoT, wearables | Internet |

### Frequency Hopping

Many RC protocols use frequency hopping to avoid interference:

```
    Time →
    ─────────────────────────────────────────────────────→
    
    │ Packet │ Packet │ Packet │ Packet │ Packet │
    │   1    │   2    │   3    │   4    │   5    │
    └────────┴────────┴────────┴────────┴────────┘
        │        │        │        │        │
        ▼        ▼        ▼        ▼        ▼
    
    Ch 65 ─ ●                            ●
    Ch 69 ─      ●                            ●
    Ch 73 ─           ●                            
    Ch 77 ─                ●                       
    
    Example: XK2 protocol hops 65→69→73→77→65...
             Period: ~5ms per packet
             Full cycle: ~20ms
```

**Sniffing hopping protocols:**
- **Fixed channel mode**: Listen on one channel, catch packets when TX hops there
- **Auto mode (mode 3)**: Automatically detect and follow the hopping pattern

### XN297 vs NRF24L01

XN297 is a Chinese clone with "scrambling" feature:

```
    Original data:     00 00 00 FF FF FF
                       (bad for radio - long runs of same bit)
                              │
                              ▼ XOR with scramble table
                              
    Scramble table:    E3 B1 4B EA 85 BC
                              │
                              ▼
                              
    Transmitted:       E3 B1 4B 15 7A 43
                       (good for radio - balanced bits)
```

**Why scrambling?**
1. **DC Balance**: Ensures ~50% ones/zeros for reliable radio transmission
2. **Clock Recovery**: Frequent bit transitions help receiver stay synchronized
3. **Spectral Spreading**: Reduces interference peaks
4. **Protocol Separation**: Basic "protection" from standard NRF24L01

In sniffer output:
- `S=Y` = Scrambled (XN297 format)
- `S=N` = Unscrambled (standard NRF24L01)

## License

Based on code from DIY-Multiprotocol-TX-Module project.
