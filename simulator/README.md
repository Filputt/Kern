# Kern Desktop Simulator

Desktop simulator for the Kern Bitcoin air-gapped signer firmware.
Renders the real LVGL UI in an SDL2 window with mouse-as-touch
input.

**Warning:** This simulator was implemented entirely by an AI
agent with only superficial human review. It must be considered
non-trusted code. Do not run it on a machine holding any
sensitive credentials (Bitcoin keys, GPG keys, SSH keys,
passwords, etc.). It is strongly advised to run it on a
dedicated machine. You can use `ssh -X` to forward only the
display over SSH while keeping the simulator isolated from
your main workstation.

## Prerequisites

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake libsdl2-dev libmbedtls-dev
```

### macOS

```bash
brew install cmake sdl2 mbedtls
```

## Build

### Linux

```bash
cd simulator \
  && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug \
  && cmake --build build -- -j$(nproc)
```

### macOS

```bash
cd simulator \
  && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_PREFIX_PATH="$(brew --prefix mbedtls);$(brew --prefix sdl2)" \
  && cmake --build build -- -j"$(sysctl -n hw.ncpu)"
```

Or with just (from the repo root):

```bash
just sim-build
```

## Run

### Linux

```bash
./simulator/build/kern_simulator
# or, from the repo root (builds with webcam support and runs with --webcam):
just sim
# without camera:
just sim-no-cam
```

### macOS

```bash
./simulator/build/kern_simulator
# or, from the repo root (builds with webcam support and runs with --webcam):
just sim
# without camera:
just sim-no-cam
```

## CLI Options

| Option              | Description                                                           |
| ------------------- | --------------------------------------------------------------------- |
| `--qr-image <path>` | Load a single QR image for camera sim                                 |
| `--qr-dir <path>`   | Load QR images from dir (cycled through)                              |
| `--data-dir <path>` | Base data directory (default: `sim_data/`)                            |
| `--width <N>`       | Display width in pixels (default: 720)                                |
| `--height <N>`      | Display height in pixels (default: 720)                               |
| `--webcam [device]` | Use webcam (default: `/dev/video0`). Requires `-DSIM_WEBCAM=ON` build |
| `--verbose`         | Enable DEBUG-level logging                                            |
| `--help`            | Show usage and exit                                                   |

## Examples

The `just` recipes only take a board argument (e.g. `just sim wave_35`); CLI options are passed by running the binary directly:

```bash
# Run with webcam (default settings)
just sim

# Run without camera
just sim-no-cam

# Run with a QR code image
just sim-qr path/to/qr.png

# Run with a directory of QR images (cycled)
./simulator/build/kern_simulator --qr-dir path/to/qr-images/

# Run with custom data directory
./simulator/build/kern_simulator --data-dir /tmp/kern-sim-data

# Run with custom resolution
./simulator/build/kern_simulator --width 480 --height 480

# Combine options
./simulator/build/kern_simulator --qr-image path/to/qr.png --data-dir /tmp/kern-sim-data
```

## Data Directory Layout

```
sim_data/                 # default base data directory
  nvs/                    # Simulated NVS storage
  spiffs/                 # Simulated flash/SPIFFS storage
  sdcard/                 # Simulated SD card
    kern/
      mnemonics/
      descriptors/
```

When `--data-dir <path>` is specified:
- NVS data goes to `<path>/nvs/`
- Flash data goes to `<path>/spiffs/`
- SD card data goes to `<path>/sdcard/kern/` (mnemonics and
  descriptors subdirectories under it)

Settings persist across runs in the NVS files.
Delete `sim_data/` (or the custom `--data-dir`) to reset to
factory state.

## Webcam Support (Optional)

Build with real webcam capture to scan QR codes and generate
real entropy (V4L2 on Linux, AVFoundation on macOS). This is
what `just sim` does; the manual steps:

### Linux

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DSIM_WEBCAM=ON
cmake --build build -- -j$(nproc)
./build/kern_simulator --webcam
# Or specify a device:
./build/kern_simulator --webcam /dev/video1
```

Your user must be in the `video` group to access the webcam device:

```bash
sudo usermod -aG video $USER   # then log out/in
```

### macOS

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DSIM_WEBCAM=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix mbedtls);$(brew --prefix sdl2)"
cmake --build build -- -j"$(sysctl -n hw.ncpu)"
./build/kern_simulator --webcam
# Or specify a device by index:
./build/kern_simulator --webcam 0
```

On macOS, the first run will prompt for Camera permission. If the
permission dialog does not appear, enable camera access for your
terminal in:

- System Settings → Privacy & Security → Camera

When `--webcam` is passed but the device cannot be opened, the
simulator falls back to blank-frame mode.

## Build-Time Resolution Override

```bash
cmake -B build -S . -DSIM_LCD_H_RES=480 -DSIM_LCD_V_RES=480
```

## Troubleshooting

**White screen over SSH X forwarding (`ssh -X`):**

```bash
SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software ./simulator/build/kern_simulator
```

The RANDR extension is not available over forwarded X11.
Forcing the software renderer works around this. The `just sim`
recipes already set these variables.

## Known Limitations

- In raw cmake builds camera simulation is file-based unless
  built with `-DSIM_WEBCAM=ON` (`just sim` builds with it)
- eFuse HMAC uses a hardcoded test key (anti-phishing
  words differ from real device, and PIN-derived keys are
  trivially recoverable from `sim_data/`)
- The simulator links against the host system's mbedTLS
  (typically 2.x via `libmbedtls-dev`) through a small
  compatibility shim. This is *not guaranteed* to be
  bit-for-bit identical to the ESP-IDF-bundled mbedTLS used
  on the real device. Do not assume that KEF blobs, PIN
  hashes, or any other cryptographic output produced by the
  simulator will round-trip on a physical Kern device.
- PPA rotation may not match hardware exactly
- Webcam support differs by OS (V4L2 on Linux, AVFoundation on macOS)
