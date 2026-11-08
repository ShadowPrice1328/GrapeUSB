#ifndef USB_H
#define USB_H

#include "iso.h"

typedef enum {
    SPACE_PREPARE_ERROR = -1,
    SPACE_PREPARE_CANCELLED = 0,
    SPACE_PREPARE_READY = 1,
    SPACE_PREPARE_BACK = 2
} SpacePrepareResult;

typedef enum {
    CREATE_ERROR = -1,
    CREATE_CANCELLED = 0,
    CREATE_SUCCESS = 1,
    CREATE_BACK = 2
} CreateResult;

typedef struct {
    char name[64];
    char size[32];
    char model[128];
    char dev_path[128];
    char part_path[128];
} UsbDevice;

int mountUSB(UsbDevice *dev);
int unmountUSB();
CreateResult create_bootable(const char *iso, UsbDevice *dev, IsoType type);
int detectVentoyDataPartition(UsbDevice *dev);

#endif