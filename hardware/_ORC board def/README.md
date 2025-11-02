# Scion ORC M4 Custom Board Definition

This directory contains the custom PlatformIO board definition for the Scion Open Reactor Controller M4 (ORC-IO-MCU).

## Board Specifications

- **MCU**: ATSAME51N20A (Cortex-M4)
- **Clock Speed**: 120 MHz
- **RAM**: 256 KB
- **Flash**: 1 MB (with bootloader offset)
- **USB VID/PID**: 0x04D8:0xEB60, 0x04D8:0xEB61

## Directory Structure

```
_ORC board def/
├── boards/
│   └── scion_orc_m4.json          # Board definition for PlatformIO
├── variants/
│   └── orc_m4/
│       ├── variant.h               # Pin definitions and mappings
│       ├── variant.cpp             # Pin configuration implementation
│       ├── pins_arduino.h          # Arduino-style pin mapping
│       └── linker_scripts/
│           └── gcc/
│               ├── flash_with_bootloader.ld     # Linker script with bootloader
│               └── flash_without_bootloader.ld  # Linker script without bootloader
└── bootloaders/
    └── orc_m4/
        ├── bootloader-orc_io_m4-v3.16.0-19-g4365018.bin
        └── update-bootloader-orc_io_m4-v3.16.0-19-g4365018.uf2
```

## PlatformIO Integration

### Automatic Setup (Already Configured)

The `orc-io-mcu/platformio.ini` is already configured to use this custom board definition:

```ini
[platformio]
boards_dir = ../hardware/_ORC board def/boards

[env:scion_orc_m4]
platform = atmelsam
board = scion_orc_m4
framework = arduino
board_build.variants_dir = ../hardware/_ORC board def/variants
lib_deps =
    khoih-prog/FlashStorage_SAMD@^1.3.2
```

**No additional setup is required** - PlatformIO will automatically find and use the custom board definition.

### How It Works

1. **`boards_dir`** - Tells PlatformIO where to find custom board JSON files
2. **`board`** - References the board name (matches `scion_orc_m4.json`)
3. **`board_build.variants_dir`** - Points to custom pin definitions and linker scripts

When you run `pio run`, PlatformIO will:
- Load the board configuration from `boards/scion_orc_m4.json`
- Use the variant files from `variants/orc_m4/` for pin mappings
- Apply the correct linker script (`flash_with_bootloader.ld`)
- Configure compiler flags for SAMD51 with FPU support

## Board Configuration Details

The `scion_orc_m4.json` file defines:

- **Build settings**: Cortex-M4 with FPU, SAMD51 core, 120MHz clock
- **Debug support**: J-Link, Atmel-ICE, OpenOCD configurations
- **Upload protocols**: SAM-BA (USB bootloader), J-Link, Atmel-ICE
- **Bootloader offset**: 0x4000 (16KB reserved for UF2 bootloader)

## Bootloader

The board uses a UF2 bootloader based on Adafruit's SAMD bootloader:

- **Version**: v3.16.0-19-g4365018
- **Offset**: 0x4000 (16KB)
- **Protocol**: SAM-BA over USB
- **Features**:
  - USB mass storage device (appears as "ORCM4BOOT")
  - 1200 baud touch to enter bootloader mode
  - Native USB support

### Installing/Updating Bootloader

To update the bootloader:

1. **Via UF2** (if bootloader already present):
   - Double-tap the reset button to enter bootloader mode
   - Drag `update-bootloader-orc_io_m4-v3.16.0-19-g4365018.uf2` to the USB drive

2. **Via J-Link/Atmel-ICE** (for initial programming):
   - Flash `bootloader-orc_io_m4-v3.16.0-19-g4365018.bin` at address 0x00000000

## Pin Definitions

Custom pin mappings are defined in `variants/orc_m4/`:

- Refer to `hardware/SAMD Pin Mux.xlsx` for complete pin assignments
- `variant.h` and `variant.cpp` define Arduino-style pin numbers
- Hardware peripherals (SPI, I2C, UART) are mapped to specific pins

## Troubleshooting

### Board not recognized

If PlatformIO doesn't recognize the board:
```bash
cd orc-io-mcu
pio run --target envdump
```

You should see: `PLATFORM: Atmel SAM > Scion Open Reactor Controller M4`

### Build fails

1. Ensure you're in the `orc-io-mcu` directory
2. Clean and rebuild:
   ```bash
   pio run --target clean
   pio run
   ```

### Upload fails

1. Check USB connection
2. Try entering bootloader mode manually (double-tap reset)
3. Verify the board appears as a USB device:
   ```bash
   ls /dev/ttyACM*
   ```

## Development

### Modifying Pin Definitions

1. Edit `variants/orc_m4/variant.h` and `variant.cpp`
2. Rebuild your project - PlatformIO will use the updated definitions

### Changing Bootloader Settings

1. Edit `boards/scion_orc_m4.json`
2. Modify `upload.offset_address` if changing bootloader size
3. Update linker script accordingly

## References

- [PlatformIO Custom Boards](https://docs.platformio.org/en/latest/platforms/creating_board.html)
- [Atmel SAM Platform](https://docs.platformio.org/en/latest/platforms/atmelsam.html)
- [ATSAME51N20A Datasheet](https://www.microchip.com/en-us/product/ATSAME51N20A)
- [Project Schematics](../Open%20Reactor%20Control%20System%20Schematics%20v1.2.pdf)
