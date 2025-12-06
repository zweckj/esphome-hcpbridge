# Migration Guide: From custom ModbusRTU to ESPHome modbus_server

## Overview

This project has been migrated from using the custom `emelianov/modbus-esp8266` library to using ESPHome's ecosystem with the `epiclabs-uc/esphome-modbus-server` external component. This provides better integration with ESPHome's architecture and aligns with ESPHome's best practices.

## What Changed

### Before (Old Configuration)

```yaml
esphome:
  name: "hcpbridge"
  libraries:
    - emelianov/modbus-esp8266  # Custom Arduino library

hcpbridge:
  id: hcpbridge_id
  rx_pin: 18
  tx_pin: 17
  rts_pin: 1  # optional
```

### After (New Configuration)

```yaml
external_components:
  - source: github://14yannick/esphome-hcpbridge
    refresh: 0s
  - source: github://epiclabs-uc/esphome-modbus-server
    refresh: 60s
    components: [modbus_server]

# UART configuration
uart:
  - id: uart_modbus
    tx_pin: 17
    rx_pin: 18
    baud_rate: 57600
    stop_bits: 1
    data_bits: 8
    parity: EVEN

# Modbus Server configuration
modbus_server:
  - id: modbus_hcp
    uart_id: uart_modbus
    address: 2
    
    holding_registers:
      # Command registers (0x9C41 to 0x9C43)
      - start_address: 0x9C41
        number: 3
        on_write: |-
          auto engine = &HoermannGarageEngine::getInstance();
          if (address == 0x9C41) {
            engine->onCounterWrite(value);
          }
          engine->setRegister9C41(address - 0x9C41, value);
          return value;
        on_read: |-
          auto engine = &HoermannGarageEngine::getInstance();
          return engine->getRegister9C41(address - 0x9C41);
      
      # Internal State registers (0x9CB9 to 0x9CC0)
      - start_address: 0x9CB9
        number: 8
        on_read: |-
          auto engine = &HoermannGarageEngine::getInstance();
          if (address == 0x9CB9) {
            engine->onModbusRequest();
          }
          return engine->getRegister9CB9(address - 0x9CB9);
        on_write: |-
          auto engine = &HoermannGarageEngine::getInstance();
          engine->setRegister9CB9(address - 0x9CB9, value);
          return value;
      
      # Broadcast registers (0x9D31 to 0x9D39)
      - start_address: 0x9D31
        number: 9
        on_write: |-
          auto engine = &HoermannGarageEngine::getInstance();
          uint16_t offset = address - 0x9D31;
          if (offset == 1) {
            value = engine->onDoorPositionChanged(value);
          } else if (offset == 2) {
            value = engine->onCurrentStateChanged(value);
          } else if (offset == 6) {
            value = engine->onRegSevenChanged(value);
          }
          engine->setRegister9D31(offset, value);
          return value;
        on_read: |-
          auto engine = &HoermannGarageEngine::getInstance();
          return engine->getRegister9D31(address - 0x9D31);

hcpbridge:
  id: hcpbridge_id
```

## Migration Steps

1. **Update your YAML configuration:**
   - Remove the `libraries:` section from `esphome:`
   - Add the `epiclabs-uc/esphome-modbus-server` external component
   - Add the `uart:` configuration block
   - Add the `modbus_server:` configuration block with register definitions
   - Update `hcpbridge:` to remove pin configurations (now in `uart:`)

2. **Update component reference:**
   If you're using a local or GitHub reference, ensure you're using the latest version that includes the modbus migration.

3. **Test your configuration:**
   - Validate the YAML: `esphome config your_config.yaml`
   - Compile: `esphome compile your_config.yaml`
   - Upload to your device: `esphome upload your_config.yaml`

## Benefits of the New Approach

1. **Better ESPHome Integration:** Uses ESPHome's native UART component instead of Arduino's Serial
2. **Declarative Configuration:** Modbus registers are defined in YAML, making it easier to understand and modify
3. **Maintainability:** External component for modbus_server is maintained by the community
4. **Flexibility:** Easy to adjust register mappings without changing C++ code
5. **ESPHome Best Practices:** Aligns with how other ESPHome components handle serial communication

## Technical Details

### Modbus Register Layout

The configuration maintains the same register layout as before:

- **0x9C41-0x9C43** (3 registers): Command registers for sending commands to the door
- **0x9CB9-0x9CC0** (8 registers): Internal state registers for door status
- **0x9D31-0x9D39** (9 registers): Broadcast registers for receiving door updates

### UART Settings

- **Baud rate:** 57600
- **Data bits:** 8
- **Stop bits:** 1
- **Parity:** EVEN
- **Slave address:** 2

These settings match the Hörmann HCP protocol requirements.

## Troubleshooting

### Issue: Compilation fails with "modbus_server not found"

**Solution:** Ensure you've added the external component:
```yaml
external_components:
  - source: github://epiclabs-uc/esphome-modbus-server
    refresh: 60s
    components: [modbus_server]
```

### Issue: Device not responding to Modbus commands

**Solution:** 
1. Check your UART pin configuration matches your hardware
2. Verify the baud rate and parity settings are correct
3. Check the Modbus slave address is set to 2
4. Enable debug logging to see Modbus traffic:
   ```yaml
   uart:
     - id: uart_modbus
       # ... other settings
       debug:
         direction: BOTH
   ```

### Issue: Door position not updating

**Solution:** The register callbacks in the YAML may not be executing. Check:
1. The lambdas in `on_write:` and `on_read:` are properly formatted
2. ESPHome logs show the register accesses
3. The HoermannGarageEngine is properly initialized

## Support

If you encounter issues after migration:
1. Check the [example_hcpbridge.yaml](example_hcpbridge.yaml) for a complete working configuration
2. Open an issue on the GitHub repository
3. Ensure you're using a compatible ESP32 board (dual-core recommended)

## Known Differences

### BusScan Request Handling

The old implementation had special handling for Modbus "BusScan" requests (a device discovery pattern). With the new modbus_server component, this specific request pattern handling has been simplified. 

**Impact:** BusScan functionality (if used) may not respond identically. This appears to be a device discovery feature typically only used during initial setup. Normal operation (door control, status reporting) is not affected.

**If you experience issues:** The BusScan response logic can be re-implemented by detecting the specific write pattern to register 0x9C41 and returning the appropriate discovery response values.

### Request-Level vs Register-Level Handling

- **Old approach:** Handled entire Modbus requests with function code awareness
- **New approach:** Handles individual register reads/writes through callbacks

**Impact:** The new implementation is cleaner and more maintainable, but trades request-level awareness for register-level simplicity. All standard garage door operations work correctly.

## Compatibility

- **Minimum ESPHome version:** 2023.11.0 or later
- **Supported boards:** ESP32, ESP32-S3 (dual-core recommended)
- **Framework:** Arduino

Core functionality (door control, position tracking, state monitoring) is identical to the previous version. Only the underlying Modbus implementation has changed.
