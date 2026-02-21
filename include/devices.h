#ifndef DEVICES_H
#define DEVICES_H

#include "usb.h"
#include <string.h>

int getUsbDevices(UsbDevice *list, int max);
int findUsbByName(const char *name, UsbDevice *devOut);
int hasEnoughSpace(const char *isoPath, UsbDevice *dev);

#endif