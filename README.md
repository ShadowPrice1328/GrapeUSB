# GrapeUSB

GrapeUSB is a minimal **CLI utility written in C** for creating bootable USB flash drives from **Windows and Linux ISO images** using [Ventoy](https://www.ventoy.net/).

It provides an interactive terminal interface, automatic USB device detection, installation and reuse of Ventoy, and ISO management.

---

## Features

- Automatic detection of removable USB devices
- Interactive terminal interface
- Automatic Ventoy installation
- Detection and reuse of existing Ventoy installations
- Support for Windows and Linux ISO images
- Multiple ISO images on a single USB drive
- ISO files are copied intact without extraction
- Available storage space verification
- Interactive ISO management:
  - Add a new ISO while keeping existing images
  - Delete all existing ISO images and replace them
- Safety checks to prevent accidental data loss
- Automatic USB partition mounting and unmounting
- Modular command execution

---

## Supported Systems

- Linux (native)

GrapeUSB requires root privileges to access and manage USB block devices.

---

## Supported ISO Types

| ISO Type                  | Method                                            |
| ------------------------- | ------------------------------------------------- |
| Windows ISO               | Copied directly to the Ventoy data partition      |
| Linux ISO                 | Copied directly to the Ventoy data partition      |
| Other bootable ISO images | Supported by Ventoy, subject to ISO compatibility |

GrapeUSB does not extract ISO contents or modify the images.

---

## How It Works

GrapeUSB uses Ventoy as its bootable USB solution.

### First-time Installation

1. Detect available removable USB devices.
2. Let the user select a USB drive.
3. Validate the selected device.
4. Install Ventoy on the selected USB drive.
5. Wait for the system to recognize the new partitions.
6. Detect and mount the Ventoy data partition.
7. Copy the selected ISO image to the USB drive.
8. Unmount the partition safely.

**Warning:** Installing Ventoy for the first time erases all existing data on the selected USB drive and recreates its partition layout.

### Existing Ventoy Installation

If Ventoy is already installed, GrapeUSB detects it and reuses the existing installation instead of formatting the drive again.

The program checks available storage space before copying a new ISO.

If there is enough space, the user can choose to:

- Add the new ISO while keeping existing images.
- Delete all existing ISO images and replace them with the new one.

If there is insufficient space, GrapeUSB asks for permission to delete all existing ISO images before continuing. Of cource you can manually remove the .iso file you don't need.

Only existing ISO files are targeted for deletion. Other files and directories are preserved.

---

## Requirements

### System

- Linux
- GCC
- Make
- Root privileges

### Required utilities

- `bash`
- `lsblk`
- `mount`
- `umount`
- `udevadm`
- `rsync`

Ventoy is bundled with the project under `vendor/ventoy/`.

---

## Build

Clone the repository and build the project:
```bash
make clean
make
```

---

## Usage

Run GrapeUSB with root privileges:
```bash
sudo ./grapeusb "path/to/.iso"
```

Follow the interactive terminal menu to:

1. Select a USB flash drive.
2. Choose an ISO image.
3. Confirm the selected device.
4. Start the creation process.

---

## Safety

GrapeUSB includes several safeguards to reduce the risk of accidental data loss:

- Only detected removable USB devices are displayed.
- USB devices are validated before Ventoy installation.
- Existing Ventoy installations are reused whenever possible.
- Explicit confirmation is required before destructive operations.
- Existing ISO images are not deleted without user consent.
- Only ISO files are targeted during ISO replacement.
- USB partitions are unmounted before device validation and after operations.
- Available storage space is checked before copying.

**Important:** Never disconnect the USB drive while GrapeUSB is operating.

---

## License

GrapeUSB is distributed under its project license.

Ventoy is distributed under the GNU General Public License v3.0 (GPL-3.0). See the bundled Ventoy license files for applicable terms.