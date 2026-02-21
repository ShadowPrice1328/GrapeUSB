#ifndef USB_H
#define USB_H

#include "iso.h"

typedef struct {
    char name[64];
    char size[16];
    char model[128];
    char dev_path[64];
    char part_path[64];
} UsbDevice;

int formatUSB(UsbDevice *dev);
int mountUSB(UsbDevice *dev);
int unmountUSB();
int create_bootable(const char *iso, UsbDevice *dev, IsoType type);

#endif