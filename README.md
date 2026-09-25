# USB Firewall / BadUSB Interceptor

A hardware-based USB HID firewall prototype designed to detect and block suspicious automated keyboard injection before it reaches a host computer.

The project creates a hardware trust boundary between an untrusted USB HID device and the computer. An intermediate Raspberry Pi Pico operates as a USB host, receives HID keyboard reports, analyzes their timing behavior, and forwards permitted input through a second Raspberry Pi Pico operating as a trusted USB HID device.

---

## Overview

USB Human Interface Devices (HID), such as keyboards, are generally trusted by an operating system after enumeration.

This behavior can be abused by devices that impersonate keyboards and generate automated keystrokes at high speed.

This project explores a hardware-level approach to this problem. Instead of allowing an external USB HID device to communicate directly with the computer, the device is placed behind an intermediate inspection layer.

The prototype analyzes HID keyboard reports in real time and uses the timing between key-press reports as a behavioral signal.

### High-Level Architecture

```text
                 UNTRUSTED USB HID DEVICE
                           |
                           | USB HID
                           v
                 +----------------------+
                 |    Raspberry Pi      |
                 |       Pico #1        |
                 |                      |
                 |      USB HOST        |
                 |                      |
                 |  HID Report Receive  |
                 |  Report Normalizing  |
                 |  Timing Analysis     |
                 |  Allow / Block       |
                 +----------+-----------+
                            |
                            | UART
                            |
                            v
                 +----------------------+
                 |    Raspberry Pi      |
                 |       Pico #2        |
                 |                      |
                 |     USB DEVICE       |
                 |                      |
                 |   Trusted HID        |
                 +----------+-----------+
                            |
                            | USB HID
                            v
                     HOST COMPUTER
```

---

## Project Goals

The main goals of this prototype are:

- Create a hardware-level USB HID inspection boundary.
- Receive keyboard HID reports before they reach the host computer.
- Analyze HID input timing in real time.
- Identify suspicious high-speed automated input.
- Block suspicious input before forwarding it.
- Forward permitted HID input through a separate trusted HID interface.
- Provide a visual indication of the current decision.
- Demonstrate the concept using a controlled HID test device.

---

## Security Problem

### BadUSB / HID Keyboard Injection

One class of USB attack involves a device pretending to be a keyboard.

The malicious device can send keyboard reports to the operating system without requiring the user to physically type the input.

A controlled HID device can therefore generate sequences such as:

```
A B C D E F G H
```

with very small intervals between reports.

This project investigates whether the behavioral characteristics of HID input, particularly timing, can be used as one signal for identifying suspicious automated keyboard injection.

> The project does not attempt to determine maliciousness solely from the identity of the USB device.

---

## Detection Concept

The current prototype uses a timing-based heuristic.

For keyboard input, Pico #1 observes successive key-press reports and measures the interval between them.

Conceptually:

```text
Key Press 1
     |
     |------ Δt ------|
                      |
                  Key Press 2
                      |
                      |------ Δt ------|
                                       |
                                   Key Press 3
```

The prototype checks whether repeated key presses occur below a configured timing threshold.

Current prototype parameters:

```c
#define FAST_INTERVAL_MS  50
#define FAST_REPORT_LIMIT 3
```

The values are configurable and are experimental parameters rather than a universal definition of malicious input.

### Detection Flow

```text
                    HID REPORT
                         |
                         v
                +----------------+
                | Normalize      |
                | HID Report     |
                +-------+--------+
                        |
                        v
                +----------------+
                | Key Press      |
                | Detection      |
                +-------+--------+
                        |
                        v
                +----------------+
                | Timing         |
                | Analysis       |
                +-------+--------+
                        |
                 +------+------+
                 |             |
              ALLOW          BLOCK
                 |             |
                 v             v
             GREEN LED      RED LED
                 |             |
                 v             X
              UART TX        STOP
                 |
                 v
             Pico #2
                 |
                 v
           USB HID Device
                 |
                 v
            Host Computer
```

### Allow Path

When an input report passes the current heuristic:

```text
HID Device → Pico #1 → Timing Analysis → ALLOW → GREEN LED
                                            |
                                            v
                                          UART → Pico #2 → USB HID → Computer
```

The permitted report is forwarded to Pico #2.

### Block Path

When repeated high-speed input reaches the configured suspicious threshold:

```text
HID Device → Pico #1 → Timing Analysis → SUSPICIOUS → RED LED → BLOCK (X)
```

The suspicious report is not forwarded through the normal UART path.

---

## Hardware Architecture

The current prototype uses two Raspberry Pi Pico boards to separate the untrusted USB side from the trusted USB device presented to the computer.

### Pico #1 — USB Host

Pico #1 operates as the USB host for the external HID device.

Responsibilities:

- Enumerate the external USB HID device.
- Receive HID reports.
- Normalize the HID report.
- Identify key-press reports.
- Measure timing between key presses.
- Apply the timing heuristic.
- Decide whether to allow or block the input.
- Control the status LEDs.
- Forward permitted reports to Pico #2 over UART.

### Pico #2 — USB HID Device

Pico #2 operates as the USB HID device connected to the computer.

Responsibilities:

- Receive data from Pico #1 over UART.
- Interpret the forwarded HID report.
- Generate the corresponding USB HID report.
- Present the trusted HID interface to the computer.

The host computer therefore communicates with Pico #2 rather than directly with the original external HID device.

### ATtiny85 — Controlled HID Test Device

The ATtiny85 USB development board is used as a controlled HID test source.

It can generate reproducible keyboard input patterns for laboratory testing, including:

- Normal keyboard-like text.
- Repeated key presses.
- Automated high-speed keyboard input.

> The ATtiny85 is a controlled test instrument used to reproduce HID behavior. It is not malware.

### Hardware Components

Current prototype hardware:

- 2 × Raspberry Pi Pico (RP2040)
- 1 × ATtiny85 USB development board
- 1 × USB-A female breakout board
- MB102 breadboard power supply
- Breadboard
- Red LEDs
- Green LEDs
- Resistors
- USB cables
- Micro-USB OTG adapter
- Jumper wires

No additional hardware is required for the current prototype architecture.

---

## Pico-to-Pico Communication

Pico #1 communicates with Pico #2 using UART.

Current configuration:

| Pico #1              | Pico #2              |
|-----------------------|-----------------------|
| UART1 TX GPIO4        | UART0 RX GPIO1        |
| GND                    | GND                    |

**Pico #1:**

```c
#define UART_ID      uart1
#define UART_BAUD    115200
#define UART_TX_PIN  4
```

**Pico #2:**

```c
#define UART_ID      uart0
#define UART_BAUD    115200
#define UART_RX_PIN  1
```

The current prototype uses a simple framing mechanism. A forwarded HID report is transmitted conceptually as:

```text
+------+------------------------+
| 'K'  | 8-byte HID report      |
+------+------------------------+
```

The `K` byte identifies the beginning of a keyboard report frame.

---

## HID Report Processing

Pico #1 receives HID reports through TinyUSB.

The prototype normalizes incoming keyboard reports into an 8-byte representation.

Conceptually:

| Byte | Field     |
|------|-----------|
| 0    | Modifier  |
| 1    | Reserved  |
| 2    | Key 1     |
| 3    | Key 2     |
| 4    | Key 3     |
| 5    | Key 4     |
| 6    | Key 5     |
| 7    | Key 6     |

The detector examines the keycode portion when determining whether the report contains a key press.

This is important because USB HID communication can also contain reports representing key release or idle states. The prototype therefore attempts to avoid treating every HID report as an independent keyboard press.

---

## Detection Algorithm

The current detection flow is:

```text
Receive HID report
        |
        v
Normalize report
        |
        v
Does report contain key press?
        |
       / \
     NO   YES
     |      |
     |      v
     |   Get current time
     |      |
     |      v
     |   Previous press?
     |      |
     |     / \
     |   NO   YES
     |   |      |
     |   |      v
     |   |   Calculate interval
     |   |      |
     |   |      v
     |   |   Interval < threshold?
     |   |      |
     |   |     / \
     |   |   NO   YES
     |   |   |      |
     |   | reset   increment
     |   | count   fast counter
     |   |          |
     |   |          v
     |   |     Limit reached?
     |   |          |
     |   |         / \
     |   |       NO   YES
     |   |       |      |
     |   |     ALLOW   BLOCK
     |   |       |      |
     +---+-------+------+
             |
             v
       Request next HID report
```

Current parameters:

```c
#define FAST_INTERVAL_MS  50
#define FAST_REPORT_LIMIT 3
```

The detector is intentionally simple at the current proof-of-concept stage.

---

## LED Status

The prototype uses visual indicators to demonstrate the decision made by the inspection layer.

| LED   | Meaning                | Description                                                              |
|-------|-------------------------|---------------------------------------------------------------------------|
| 🟢 Green | ALLOW / FORWARDED       | The report passed the current timing heuristic and is being forwarded.    |
| 🔴 Red   | SUSPICIOUS / BLOCKED    | The timing heuristic classified the input as suspicious and it is blocked.|

The LEDs are primarily a demonstration and debugging interface for the prototype.

---

## Firmware Architecture

The project contains two independent Raspberry Pi Pico firmware projects:

```text
pico-host/
pico-device/
```

### Pico #1 Firmware

Source: `pico-host/pico-host.c`

Main responsibilities:

```text
TinyUSB Host
     |
     v
HID Report Callback
     |
     v
Report Normalization
     |
     v
Timing Analysis
     |
     +-----------> BLOCK → RED LED
     |
     +-----------> ALLOW → GREEN LED → UART
```

### Pico #2 Firmware

Source: `pico-device/pico-device.c`
Additional USB descriptor source: `pico-device/usb_descriptors.c`

Main responsibilities:

```text
UART RX
   |
   v
Receive forwarded report
   |
   v
Convert to USB HID report
   |
   v
USB HID Device
   |
   v
Host Computer
```

---

## Software Stack

The project uses:

- Raspberry Pi Pico / RP2040
- Raspberry Pi Pico SDK
- TinyUSB
- C
- CMake
- GNU ARM Embedded Toolchain
- Make
- Git
- VS Code
- Kali Linux

**Development environment:**

| Component     | Version               |
|----------------|------------------------|
| Operating System | Kali GNU/Linux Rolling |
| Architecture     | x86_64                 |
| Pico SDK         | 2.3.1                  |
| CMake            | 4.3.x                  |
| ARM GCC          | 15.x                   |
| Python           | 3.x                     |

---

## Repository Structure

```text
usb-firewall-dongle/
│
├── .gitignore
├── README.md
│
├── pico-host/
│   ├── CMakeLists.txt
│   ├── pico-host.c
│   ├── pico_sdk_import.cmake
│   ├── tusb_config.h
│   └── .gitignore
│
└── pico-device/
    ├── CMakeLists.txt
    ├── pico-device.c
    ├── pico_sdk_import.cmake
    ├── tusb_config.h
    ├── usb_descriptors.c
    └── .gitignore
```

Generated build directories and firmware artifacts are excluded from version control.

---

## Build Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/Shivaraj09122005/usb-firewall-dongle.git
cd usb-firewall-dongle
```

### 2. Build Pico #1 — USB Host

From the project root:

```bash
cd pico-host
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

Generated firmware files are placed inside `pico-host/build/`.

### 3. Build Pico #2 — USB Device

From the project root:

```bash
cd pico-device
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

Generated firmware files are placed inside `pico-device/build/`.

### 4. Flashing the Raspberry Pi Pico

Put the Raspberry Pi Pico into BOOTSEL mode and connect it to the development computer. The Pico should appear as `RPI-RP2`.

The generated `.uf2` firmware can then be copied to the mounted Pico filesystem.

Example:

```bash
cp pico-host.uf2 /mnt/pico/
sync
```

The same procedure can be used for Pico #2 with its generated UF2 firmware.

---

## Testing Strategy

The prototype uses a controlled HID source to reproduce different input patterns. Testing is divided into independent stages so that each part of the system can be verified separately.

### Test 1 — ATtiny85 HID Verification

The ATtiny85 can first be connected directly to a computer to verify that its HID firmware is operating correctly.

Example output:

```
USB FIREWALL | INPUT VERIFIED
```

This verifies that the controlled HID test device is functioning.

### Test 2 — Pico #2 HID Verification

Pico #2 can be tested independently by sending UART data to its RX input.

```text
UART → Pico #2 → USB HID → Computer
```

This separates USB device functionality from the detection algorithm.

### Test 3 — Pico #1 USB Host Verification

Pico #1 receives HID reports from the external test device. This verifies the USB host and HID report reception path independently.

### Test 4 — End-to-End Verification

The complete intended path is:

```text
ATtiny85 --USB HID--> Pico #1 --Timing Analysis--> ALLOW/BLOCK --UART--> Pico #2 --USB HID--> Computer
```

This verifies the overall architecture.

---

## Controlled HID Test Firmware

The ATtiny85 can be programmed with controlled HID test firmware.

Example:

```cpp
#include "DigiKeyboard.h"

void setup()
{
    DigiKeyboard.sendKeyStroke(0);
    DigiKeyboard.delay(1000);
}

void loop()
{
    DigiKeyboard.print("USB FIREWALL | INPUT VERIFIED");
    DigiKeyboard.sendKeyStroke(KEY_ENTER);

    DigiKeyboard.delay(1200);
}
```

A separate controlled test pattern can generate repeated rapid keyboard reports to evaluate the timing detector.

> These tests are intended for authorized laboratory and demonstration use.

### Example Test Categories

**Normal Input** — A controlled HID device generates input with relatively long delays between events.

```text
Input → Pico #1 → Timing Analysis → ALLOW → GREEN → Pico #2 → Computer
```

**Automated High-Speed Input** — A controlled HID device generates repeated keyboard reports with short intervals.

```text
Automated HID → Pico #1 → Timing Analysis → Suspicious Pattern → RED / BLOCK
```

The exact classification depends on the current detector parameters and the timing observed by the firmware.

---

## Why Two Raspberry Pi Picos?

The architecture separates the untrusted USB side from the trusted USB device side.

```text
      UNTRUSTED SIDE                          TRUSTED SIDE

USB HID → Pico #1 (USB HOST) → Inspection → UART → Pico #2 (USB DEVICE) → Computer
```

This allows the prototype to inspect the external HID traffic before the computer receives the forwarded keyboard input.

The separation also avoids requiring a single RP2040 USB controller to simultaneously perform both host-side and device-side USB roles.

---

## Why Hardware-Level Inspection?

A conventional software-only approach receives keyboard input after the operating system has already accepted the USB HID device.

This project investigates moving part of the trust decision into an intermediate hardware layer.

**Traditional:**

```text
USB Device → Computer USB Stack → Operating System
```

**Prototype:**

```text
USB Device → Hardware Inspection → [BLOCK] or → Trusted HID Interface → Operating System
```

The objective is to prevent suspicious keyboard reports from being forwarded to the host computer.

---

## Security Model

The current security model treats the external HID device as untrusted.

```text
UNTRUSTED HID DEVICE
        |
        v
+----------------------+
| HID Inspection       |
|                      |
| Report Normalization |
| Timing Analysis      |
+----------+-----------+
           |
      +----+----+
      |         |
    ALLOW      BLOCK
      |         |
      v         v
    UART       STOP
      |
      v
 Trusted HID
      |
      v
    HOST
```

Only reports that pass the current heuristic are intended to be forwarded.

---

## Current Limitations

This is a research and development prototype.

1. **Timing is only one signal.** The current detector primarily examines timing between key presses. A sophisticated automated device could potentially attempt to mimic human-like timing.
2. **False positives are possible.** Fast legitimate keyboard input could potentially trigger the timing heuristic. Threshold selection therefore requires additional experimentation.
3. **False negatives are possible.** An automated device that deliberately introduces delays could potentially avoid the current timing threshold.
4. **HID-focused architecture.** The current implementation focuses on USB HID keyboard traffic. It is not a general-purpose USB storage firewall. For example, a USB flash drive containing images or documents is not exposed through this current HID-only architecture.
5. **No file malware scanning.** The prototype does not inspect files stored on USB storage devices and does not replace antivirus or endpoint security software.
6. **HID report compatibility.** Different HID devices can use different report descriptors and report formats. A more generalized implementation would require broader HID descriptor handling and compatibility testing.
7. **Prototype thresholds.** The current timing values (`FAST_INTERVAL_MS = 50`, `FAST_REPORT_LIMIT = 3`) are experimental and should not be interpreted as universal security thresholds.

---

## What the Prototype Can Demonstrate

- USB HID host interception.
- HID keyboard report reception.
- HID report normalization.
- Timing-based behavioral analysis.
- Allow/block decision logic.
- Hardware LED status indication.
- UART communication between two microcontrollers.
- USB HID device emulation.
- Separation between an external HID device and the host computer.
- Controlled reproduction of automated HID input.

## What the Prototype Does Not Claim

This project does not claim to:

- Detect every BadUSB attack.
- Prove that a USB device is malicious.
- Detect malware inside USB storage.
- Scan USB files.
- Replace antivirus software.
- Replace endpoint security software.
- Guarantee zero false positives.
- Guarantee zero false negatives.
- Protect against every possible USB attack technique.

The current implementation is a research prototype focused specifically on HID keyboard injection behavior.

---

## Research and Future Development

Potential future development areas include:

- Adaptive timing thresholds.
- Statistical typing behavior analysis.
- Key-press and key-release sequence analysis.
- HID report-rate analysis.
- Burst detection.
- Modifier-key behavior analysis.
- HID descriptor validation.
- Device-class validation.
- Stateful behavioral models.
- Configurable security policies.
- Event logging.
- Security event counters.
- Improved false-positive handling.
- Broader HID compatibility.
- More extensive automated testing.
- USB protocol-level analysis.

These are potential research directions and are not claimed as implemented features unless present in the corresponding firmware.

---

## Engineering Challenges

The project involves several embedded-security challenges:

- **USB Host Operation** — Pico #1 must operate as a USB host and correctly receive HID reports from an external device.
- **USB Device Emulation** — Pico #2 must present a valid HID interface to the computer.
- **HID Report Handling** — Different HID devices can produce different report structures and lengths.
- **Real-Time Detection** — The inspection logic must analyze reports without introducing excessive latency.
- **False Positives** — Legitimate fast keyboard input must be considered when selecting detection thresholds.
- **False Negatives** — Automated devices can potentially alter their timing behavior to avoid simple threshold-based detection.
- **Hardware Separation** — The architecture must maintain a clear boundary between the untrusted USB device and the host computer.

---

## Project Status

**Current stage:** Proof-of-Concept / Hardware Security Prototype

Implemented architecture includes:

- Raspberry Pi Pico #1 USB host.
- Raspberry Pi Pico #2 USB HID device.
- TinyUSB integration.
- HID report reception.
- HID report processing.
- UART communication between Pico boards.
- Timing-based detection logic.
- Allow/block decision path.
- LED status indication.
- Controlled ATtiny85 HID test source.
- End-to-end prototype architecture.

The project remains under development and testing.

---

## Demonstration Architecture

The intended demonstration consists of:

**Stage 1 — Controlled HID Source**
```text
ATtiny85 → Keyboard HID Reports
```

**Stage 2 — Hardware Inspection**
```text
Pico #1: Receive → Analyze → ALLOW / BLOCK
```

**Stage 3 — Trusted Output**
```text
Pico #1 (Allowed Reports) → Pico #2 → Computer
```

Visual indication:

- 🟢 **GREEN** → Permitted / Forwarded
- 🔴 **RED** → Suspicious / Blocked

### Example End-to-End Flow

```text
                       CONTROLLED TEST
                              |
                              v
                         ATtiny85 HID
                              |
                              | USB HID
                              v
                  +------------------------+
                  |       PICO #1          |
                  |                        |
                  |       USB HOST         |
                  |                        |
                  |  HID Report Receive    |
                  |  Report Normalization  |
                  |  Timing Analysis       |
                  +-----------+------------+
                              |
                    +---------+---------+
                    |                   |
                  ALLOW               BLOCK
                    |                   |
                    v                   v
                 GREEN                 RED
                    |                   |
                    v                   X
                  UART
                    |
                    v
              +-----------+
              |  PICO #2  |
              | USB HID   |
              +-----+-----+
                    |
                    v
               COMPUTER
```

---

## Project Objectives

The broader objective is to investigate whether a low-cost embedded device can provide an additional hardware trust boundary for USB HID input.

The prototype focuses on the engineering trade-offs between:

- Security
- Detection Accuracy
- USB Compatibility
- Low Latency
- Low Hardware Complexity

A practical HID security mechanism must balance suspicious-input detection with legitimate keyboard usability.

---

## Reproducibility

The project is intended to provide a reproducible embedded-security development environment.

The repository contains the primary firmware source and configuration files required to rebuild the Pico firmware.

Generated build artifacts are intentionally excluded from version control.

---

## License

This project is released under the MIT License. See the [LICENSE](LICENSE) file for the complete license terms.

---

## Disclaimer

This project is an educational and research prototype.

The controlled HID injection tests are intended for authorized laboratory, development, and demonstration environments only.

The timing heuristic should not be interpreted as definitive proof that a USB device is malicious or safe.

---

## Author

**Shivaraj**
Embedded Systems | Hardware Security | USB Security

GitHub: [github.com/Shivaraj09122005/usb-firewall-dongle](https://github.com/Shivaraj09122005/usb-firewall-dongle)
