# esphome-hcpbridge

[![GitHub](https://img.shields.io/github/license/14yannick/esphome-hcpbridge)](https://github.com/14yannick/esphome-hcpbridge/blob/main/LICENSE)
[![GitHub issues](https://img.shields.io/github/issues/14yannick/esphome-hcpbridge)](https://github.com/14yannick/esphome-hcpbridge)
[![GitHub Sponsors](https://img.shields.io/github/sponsors/mapero)](https://github.com/sponsors/mapero)
![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/14yannick/esphome-hcpbridge/build.yaml)


This is a esphome-based adaption of the HCPBridge. thanks to [mapero](https://github.com/14yannick/esphome-hcpbridge) for the initial esphome port. Credits for the initial development of the HCPBridge go to [Gifford47](https://github.com/Gifford47/HCPBridgeMqtt), [hkiam](https://github.com/hkiam/HCPBridge) and all the other guys contributed.

## Usage

### Example esphome configuration

```YAML
substitutions:
  name: "hcpbridge"
  friendly_name: "Garage Door"
esphome:
  name: "${name}"
  friendly_name: "${friendly_name}"
  libraries:
    - emelianov/modbus-esp8266 # Required for communication with the modbus
  platformio_options:
    board_build.f_cpu: 240000000L

external_components:
    source: github://14yannick/esphome-hcpbridge
    refresh: 0s # Ensure you always get the latest version

esp32:
  board: #adafruit_feather_esp32s3 #set your board
  framework:
    type: arduino

hcpbridge:
  id: hcpbridge_id
  rx_pin: 18 # optional, default=18
  tx_pin: 17 # optional, default=17
  #rts_pin : 1 # optional RTS pin to use if hardware automatic control flow is not available.

cover:
  - platform: hcpbridge
    name: ${friendly_name}
    device_class: garage
    id: garagedoor_cover
```

### Home Assistant

![Home Assistant Device Overview](docs/device_overview.png)

### Cover

The component provides a cover component to control the garage door.

### Light

The component provides a Light component to turn the light off and on.
The Output is needed to control the light.
```YAML
output:
  - platform: hcpbridge
    id: output_light

light:
  - platform: hcpbridge
    id: gd_light
    output: output_light
    name: Garage Door Light
```
### Binary_Sensor

The component provides you two sensor.

- `is_connected`: Who indicated if there is a valid connection with the door.
- `relay_state`: Give the status of the option relay (Menu 30) of the HCP.
```YAML
binary_sensor:
  - platform: hcpbridge
    is_connected:
      name: "HCPBridge Connected"
      id: sensor_connected
    relay_state:
      name: "Garage Door Relay state"
      id: sensor_relay
      #on_state:
      #create your automation based on Garage Door Relay state
```
### Text_sensor

This component provide you a detailed current state of the door. This text can be changed using the substitute functionality.
```YAML
text_sensor:
  - platform: hcpbridge
    id: sensor_templ_state
    name: "Garage Door State"
```
### sensor

This component provide you the position of the door in %. Where 100% is fully open.
```YAML
sensor:
  - platform: hcpbridge
    id: sensor_position
    name: ${sen_pos}
```
### Button

This component allows you to add three buttons to sond commands to the door.
```YAML
button:
  - platform: hcpbridge
    vent_button:
      id: button_vent
      name: "Garage Door Vent"
    impulse_button:
      id: button_impulse
      name: "Garage Door Impulse"
    half_button:
      id: button_half
      name: "Half"
```

### Switch

This component allows you to add two switch to sond commands to the door.
```YAML
switch:
  - platform: hcpbridge
    vent_switch:
      id: switch_vent
      name: "Venting"
      restore_mode: disabled
    half_switch:
      id: half_switch
      name: "Open Half"
      restore_mode: disabled
```

### Services

Additionally, when using the cover component, you can expose the following services to the API:

- `esphome.hcpbridge_go_to_close`: To close the garage door
- `esphome.hcpbridge_go_to_half`: To move the garage door to half position
- `esphome.hcpbridge_go_to_vent`: To move the garage door to the vent position
- `esphome.hcpbridge_go_to_open`: To open the garage door
- `esphome.hcpbridge_toggle`: Send an Impulse command to the door

There are in the YAML and not directly in the Cover to remove the API dependency there. This give the possibility to use the Cover without the API Component for exemple only with the web_server or mqtt.
```YAML
api:
  encryption:
    key: !secret api_key
  services:
    - service: go_to_open
      then:
        - lambda: |-
            id(garagedoor_cover).on_go_to_open();
    - service: go_to_close
      then:
        - lambda: |-
            id(garagedoor_cover).on_go_to_close();
    - service: go_to_half
      then:
        - lambda: |-
            id(garagedoor_cover).on_go_to_half();
    - service: go_to_vent
      then:
        - lambda: |-
            id(garagedoor_cover).on_go_to_vent();
    - service: toggle
      then:
        - cover.toggle: garagedoor_cover
```

### Example YAML

Check out the [example_hcpbridge.yaml](./example_hcpbridge.yaml) for a complete yaml with all hcpbridge components.

# Project

- HCPBridge from `Tysonpower` on an `Hörmann Promatic 4`

You can find more information on the project here: [Hörmann garage door via MQTT](https://community.home-assistant.io/t/hormann-garage-door-via-mqtt/279938/340)
Known working hardware are the ESP32 and S3 dual core chip.

# ToDo

- [x] Initial working version
- [ ] Use esphome modbus component instead of own code (see [Migration Analysis](#migration-to-esphome-native-modbus) below)
- [x] Map additional functions to esphome
- [x] Use callbacks instead of pollingComponent (Only hcpbridge is polling)
- [x] Expert options for the HCPBridge component (GPIOs ...)

# Migration to ESPHome Native Modbus

This section documents why the project currently cannot migrate away from the third-party `emelianov/modbus-esp8266` library to ESPHome's native `modbus`/`modbus_controller` components.

## Current Architecture

The ESP32 acts as a **Modbus RTU slave** (address 2) communicating with the Hörmann garage door controller (the master) over RS485 at 57600 baud, 8E1. The third-party library provides:

- **Function Code 23 (0x17) — Read/Write Multiple Registers**: The Hörmann HCP protocol uses FC23 as its primary communication method. In a single Modbus frame, the master writes command registers (`0x9C41`) and reads state registers (`0x9CB9`) simultaneously.
- **`onRequest` callback**: Before responding to a request, the code dynamically prepares response data based on the request type (command request, bus scan, or empty command).
- **`onSet` per-register callbacks**: When the master writes specific registers (e.g., broadcast state at `0x9D31`), callbacks fire to decode door position, state changes, and light/relay status in real-time.
- **Dedicated FreeRTOS task**: Modbus handling runs on a separate high-priority task pinned to core 1 to meet the strict timing requirements of the HCP protocol.

## ESPHome Native Modbus Capabilities (as of 2025.x / 2026.x)

ESPHome supports Modbus RTU server mode via `role: server` in the `modbus` component. However, its server implementation only handles the following function codes:

| Function Code | Description | Supported |
|---|---|---|
| 0x03 | Read Holding Registers | ✅ |
| 0x04 | Read Input Registers | ✅ |
| 0x06 | Write Single Register | ✅ |
| 0x10 | Write Multiple Registers | ✅ |
| **0x17** | **Read/Write Multiple Registers** | **❌ "not implemented"** |

(Source: [ESPHome `modbus_definitions.h` Line 37](https://github.com/esphome/esphome/blob/dev/esphome/components/modbus/modbus_definitions.h#L37) — `READ_WRITE_MULTIPLE_REGISTERS = 0x17, // not implemented`)

## Gap Analysis

| Feature Required by HCPBridge | ESPHome Native Support | Status |
|---|---|---|
| FC23 Read/Write Multiple Registers (0x17) | ❌ Not implemented | **CRITICAL — Blocks migration** |
| Pre-response callback (`onRequest`) to prepare register values before reply | ❌ Not available | **CRITICAL — Blocks migration** |
| Per-register write callbacks (`onSet`) for real-time state decoding | Partial (`write_lambda` on `ServerRegister`) | Significant gap |
| Dedicated high-priority FreeRTOS task for Modbus I/O | ❌ Runs in main loop | Significant gap |
| Direct register read/write with timing-based command sequences | ❌ Simplified register model | Significant gap |

## What Would Need to Change in ESPHome

To enable migration, the following features would need to be added to ESPHome's native Modbus component:

1. **FC23 support in server mode**: Parse incoming Read/Write Multiple Registers requests and dispatch them to device handlers. This is the single most critical missing feature.
2. **Pre-response callback**: Allow devices to dynamically prepare response register data after receiving a request but before the response frame is sent.
3. **Per-register write notification**: Fire callbacks when specific holding registers are written by the master, passing both old and new values.
4. **Configurable task scheduling**: Allow Modbus processing on a dedicated FreeRTOS task for timing-critical protocols.

## Why Not Split FC23 into FC16 + FC04?

A natural question is whether FC23 (Read/Write Multiple Registers) could be decomposed into two separate operations that ESPHome *does* support — for example FC16 (0x10, Write Multiple Registers) + FC04 (0x04, Read Input Registers).

**This is not possible because the ESP32 is the Modbus slave, not the master.** The Hörmann garage door controller is the Modbus master — it decides which function codes to send. The ESP32 must respond to whatever the master sends ([`mb.slave(SLAVE_ID)`](components/hcpbridge/hoermann.cpp#L47)).

In the HCP protocol, the master sends FC23 frames to the ESP32 slave. The slave cannot tell the master to use different function codes. This is a fixed hardware protocol defined by Hörmann, not something configurable on the ESP side.

Specifically, looking at the [`onRequest` handler](components/hcpbridge/hoermann.cpp#L94-L140):

```
Master → Slave (FC23): Write 2 regs at 0x9C41, Read 8 regs at 0x9CB9   (command cycle)
Master → Slave (FC23): Write 2 regs at 0x9C41, Read 2 regs at 0x9CB9   (empty command)
Master → Slave (FC23): Write 3 regs at 0x9C41, Read 5 regs at 0x9CB9   (bus scan)
Master → Slave (FC16): Write 9 regs at 0x9D31                          (broadcast state)
```

The first three message types all use FC23 — these are initiated by the Hörmann controller and cannot be changed. Only the broadcast state update uses FC16, which ESPHome already supports.

## Conclusion

The migration is **not currently feasible**. The Hörmann garage door controller (Modbus master) uses Function Code 23 (Read/Write Multiple Registers) for its primary communication, and the ESP32 (Modbus slave) must support it. Since ESPHome does not implement FC23 in server mode, this project must continue using the `emelianov/modbus-esp8266` library. We cannot work around this by splitting FC23 into separate function codes because the master — not the slave — determines which function codes are used.

# Contribute

I am open for contribution. Just get in contact with me.

# License

```
MIT License

Copyright (c) 2023 Jochen Scheib

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
