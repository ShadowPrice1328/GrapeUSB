#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "exec.h"
#include "usb.h"
#include "iso.h"
#include "utils.h"

#define MNT_USB_PATH "/mnt/grapeusb_usb"

int formatUSB(UsbDevice *dev)
{
    if (dev->part_path[0] == '\0')
    {
        fprintf(stderr, "Partition path not set!\n");
        return -1;
    }

    char *cmd[] = {
        "mkfs.vfat",
        "-F32",
        dev->part_path,
        NULL
    };

    return run_checked(cmd);
}

int mountUSB(UsbDevice *dev)
{
    if (access(MNT_USB_PATH, F_OK) != 0)
        mkdir(MNT_USB_PATH, 0755);

    char *cmd[] = {"mount", dev->part_path, MNT_USB_PATH, NULL};
    return run_checked(cmd);
}

int unmountUSB()
{
    char *cmd[] = {"umount", MNT_USB_PATH, NULL};
    return run(cmd);
}

int create_bootable(const char *iso, UsbDevice *dev, IsoType isoType) 
{
    int iso_mounted = 0;
    int usb_mounted = 0;

    unmountISO();
    unmountUSB();

    if (mountISO(iso) != 0)
        goto error;
    iso_mounted = 1;

    if (formatUSB(dev) != 0)
        goto error;

    if (mountUSB(dev) != 0)
        goto error;
    usb_mounted = 1;

    if (copyFiles(isoType) != 0)
        goto error;

    if (usb_mounted)
        unmountUSB(dev);

    if (iso_mounted)
        unmountISO();

    return 0;

error:
    if (usb_mounted)
        unmountUSB(dev);

    if (iso_mounted)
        unmountISO();

    return -1;
}