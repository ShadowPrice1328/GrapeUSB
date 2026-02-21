#ifndef UI_H
#define UI_H

#include "usb.h"

typedef enum {
    MENU,
    MAIN_INFO,
    DEVICES,
    BEGIN,
    START,
    EXIT
} Screen;

void clearScreen();
Screen showDevices(UsbDevice *dev_data);
Screen showBeginCreation(UsbDevice *dev_data, IsoType isoType);
Screen showStartCreation(UsbDevice *dev_data, const char* iso, IsoType isoType);
Screen showMainInfo();
Screen showMenu();

#endif