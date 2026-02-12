# GrapeUSB

GrapeUSB is a minimal, safe and extensible CLI utility for creating **bootable USB flash drives** from **Windows and Linux ISO images**.

The tool provides an interactive terminal interface, automatic USB device detection, controlled disk formatting, and reliable creation of bootable media.

---

## Features

- Automatic detection of removable USB devices  
- Interactive terminal menu  
- GPT partitioning and FAT32 formatting  
- Windows bootable USB creation  
- Linux bootable USB creation  
- Safety checks to prevent accidental data loss  
- Automatic cleanup and rollback  
- Modular command execution  

---

## Supported Systems

- Linux (native)
- Windows via WSL

---

## Supported ISO Types

| ISO Type   | Method                                   |
|-------------|-------------------------------------------|
| Windows ISO | Partition + file copy + optional WIM split |
| Linux ISO   | Hybrid ISO direct write or file copy      |
| Hybrid ISO  | Automatic detection                       |

---

## How It Works

### Windows ISO

1. USB device is unmounted  
2. Filesystems and partitions are wiped  
3. GPT partition table is created  
4. EFI FAT32 partition is created  
5. ISO image is mounted  
6. Files are copied to USB  
7. Large `install.wim` is split if required  
8. USB becomes bootable  

### Linux ISO

- Hybrid ISO images are written directly  
- Non-hybrid ISOs are processed using partition + file copy method  

---

## Requirements

- Linux system  
- Root privileges  

### Required utilities

- parted  
- wipefs  
- lsblk  
- mount  
- cp  
- wimlib-imagex (Windows only)  

---

## Build

```bash
gcc main.c -o grapeusb
```
## Usage

