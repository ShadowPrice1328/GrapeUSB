#include <stdio.h>
#include "exec.h"
#include "usb.h"
#include "iso.h"

#define MNT_USB "/mnt/grapeusb_usb"

int formatUSB(UsbDevice *dev)
{
    char *cmd[] = {"mkfs.vfat", "-F32", dev->dev_path, NULL};
    return run_checked(cmd);
}

int mountUSB(UsbDevice *dev)
{
    char *cmd[] = {"mount", dev->part_path, MNT_USB, NULL};
    return run_checked(cmd);
}

int unmountUSB()
{
    char *cmd[] = {"umount", MNT_USB, NULL};
    return run(cmd);
}

int create_bootable(const char *iso, UsbDevice *dev, IsoType type)
{
    int iso_mounted = 0;
    int usb_mounted = 0;

    if (mountISO(iso) != 0)
        goto error;
    iso_mounted = 1;

    if (formatUSB(dev) != 0)
        goto error;

    if (mountUSB(dev) != 0)
        goto error;
    usb_mounted = 1;

    char *copy[] = {"cp", "-r", "/mnt/grapeusb_iso/.", MNT_USB, NULL};
    if (run_checked(copy) != 0)
        goto error;

    unmountUSB();
    unmountISO();
    return 0;

error:
    if (usb_mounted)
        unmountUSB();
    if (iso_mounted)
        unmountISO();
    return -1;
}