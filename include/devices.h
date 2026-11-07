#ifndef DEVICES_H
#define DEVICES_H

#include "usb.h"

int getUsbDevices(UsbDevice *list, int max);
int findUsbByName(const char *name, UsbDevice *devOut);
int validateUsbDeviceForVentoy(UsbDevice *dev);

#endif