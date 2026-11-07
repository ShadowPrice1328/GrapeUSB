
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <limits.h>

#include "exec.h"
#include "usb.h"
#include "iso.h"
#include "ventoy.h"
#include "devices.h"

#define MNT_USB_PATH "/mnt/grapeusb_usb"

int unmountDevicePartitions(const char *device)
{
    char command[256];

    snprintf(command, sizeof(command),
             "lsblk -nrpo MOUNTPOINT %s", device);

    FILE *fp = popen(command, "r");
    if (!fp)
        return -1;

    char line[512];
    char mounts[16][256];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < 16)
    {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] != '\0')
            snprintf(mounts[count++], sizeof(mounts[0]), "%s", line);
    }

    int status = pclose(fp);
    if (status != 0)
        return -1;

    for (int i = count - 1; i >= 0; i--)
    {
        char *argv[] = {"umount", mounts[i], NULL};
        if (run_checked(argv) != 0)
            return -1;

        printf("Unmounted: %s\n", mounts[i]);
    }

    return 0;
}

static int deleteOldISOs(void)
{
    const char *mount_path = "/mnt/grapeusb_usb";

    DIR *dir = opendir(mount_path);
    if (dir == NULL)
    {
        perror("Cannot open Ventoy partition");
        return -1;
    }

    struct dirent *entry;
    int deleted = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        const char *ext = strrchr(entry->d_name, '.');

        if (ext == NULL || strcasecmp(ext, ".iso") != 0)
            continue;

        char path[PATH_MAX];

        int len = snprintf(path, sizeof(path), "%s/%s",
                           mount_path, entry->d_name);

        if (len < 0 || (size_t)len >= sizeof(path))
            continue;

        struct stat st;

        if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        if (unlink(path) != 0)
        {
            perror("Cannot delete old ISO");
            closedir(dir);
            return -1;
        }

        printf("Deleted: %s\n", entry->d_name);
        deleted++;
    }

    closedir(dir);

    printf("Deleted %d old ISO file(s).\n", deleted);
    return 0;
}

int detectVentoyDataPartition(UsbDevice *dev)
{
    if (dev == NULL || dev->dev_path[0] == '\0')
        return 0;

    char command[256];

    snprintf(command, sizeof(command),
             "lsblk -nrpo NAME,TYPE,FSTYPE,LABEL,PKNAME %s",
             dev->dev_path);

    FILE *fp = popen(command, "r");
    if (fp == NULL)
        return 0;

    char line[512];
    int found = 0;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char path[128] = "";
        char type[32] = "";
        char fstype[32] = "";
        char label[64] = "";
        char parent[64] = "";

        if (sscanf(line, "%127s %31s %31s %63s %63s",
                   path, type, fstype, label, parent) != 5)
            continue;

        if (strcmp(type, "part") == 0 &&
            strcmp(fstype, "exfat") == 0 &&
            strcmp(label, "Ventoy") == 0 &&
            strcmp(parent, dev->dev_path) == 0)
        {
            snprintf(dev->part_path, sizeof(dev->part_path),
                     "%s", path);

            found = 1;
            break;
        }
    }

    int status = pclose(fp);

    if (status != 0)
        return 0;

    return found;
}

int mountUSB(UsbDevice *dev)
{
    if (dev == NULL || dev->part_path[0] == '\0')
    {
        fprintf(stderr, "Invalid USB partition\n");
        return -1;
    }

    struct stat st;

    if (mkdir(MNT_USB_PATH, 0755) != 0)
    {
        if (errno != EEXIST)
        {
            perror("Cannot create USB mount directory");
            return -1;
        }
    }

    if (lstat(MNT_USB_PATH, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Mount path is not a directory\n");
        return -1;
    }

    char *cmd[] = {
        "mount",
        dev->part_path,
        MNT_USB_PATH,
        NULL
    };

    return run_checked(cmd);
}

int unmountUSB()
{
    if (umount2("/mnt/grapeusb_usb", 0) == 0)
        return 0;

    // Розділ уже розмонтований — це нормально.
    if (errno == EINVAL || errno == ENOENT)
        return 0;

    perror("Failed to unmount USB");
    return -1;
}


static int copyISO(const char *iso)
{
    printf("\nCopying ISO to Ventoy partition...\n\n");
    fflush(stdout);

    char *cmd[] = {
        "rsync",
        "--info=progress2",
        "--human-readable",
        "--",
        (char *)iso,
        MNT_USB_PATH "/",
        NULL
    };

    if (run_checked(cmd) != 0)
    {
        fprintf(stderr, "ISO copy failed\n");
        return -1;
    }

    printf("\nSynchronizing USB storage...\n");

    char *sync_cmd[] = {"sync", NULL};

    if (run_checked(sync_cmd) != 0)
    {
        fprintf(stderr, "Synchronization failed\n");
        return -1;
    }

    return 0;
}

static int hasEnoughSpaceOnMountedUSB(const char *iso)
{
    struct stat iso_stat;
    struct statvfs fs_stat;

    if (stat(iso, &iso_stat) != 0 || !S_ISREG(iso_stat.st_mode))
    {
        perror("Cannot access ISO file");
        return -1;
    }

    if (statvfs(MNT_USB_PATH, &fs_stat) != 0)
    {
        perror("Cannot read USB filesystem information");
        return -1;
    }

    unsigned long long available =
        (unsigned long long)fs_stat.f_bavail * fs_stat.f_frsize;

    unsigned long long required = (unsigned long long)iso_stat.st_size;

    printf("ISO size: %llu bytes\n", required);
    printf("Available space: %llu bytes\n", available);

    if (required > available)
    {
        fprintf(stderr, "Not enough space on Ventoy partition\n");
        return 0;
    }

    return 1;
}


int create_bootable(const char *iso, UsbDevice *dev, IsoType isoType)
{
    (void)isoType;

    if (iso == NULL || dev == NULL)
    {
        fprintf(stderr, "Invalid arguments\n");
        return -1;
    }

    struct stat st;

    if (stat(iso, &st) != 0 || !S_ISREG(st.st_mode))
    {
        fprintf(stderr, "Invalid ISO file\n");
        return -1;
    }

    int ventoy_status = detectVentoyDataPartition(dev);

    if (ventoy_status < 0)
    {
        fprintf(stderr, "Cannot inspect USB device\n");
        return -1;
    }

    if (ventoy_status == 1)
    {
        printf("\nExisting Ventoy installation detected.\n");
        printf("Reusing existing Ventoy partition.\n");

        if (unmountUSB() != 0)
        {
            fprintf(stderr, "Cannot unmount previous USB mount.\n");
            return 0;
        }

        if (unmountDevicePartitions(dev->dev_path) != 0)
        {
            fprintf(stderr, "Failed to unmount USB partitions.\n");
            return 0;
        }

        if (validateUsbDeviceForVentoy(dev) != 0)
        {
            fprintf(stderr, "USB device validation failed\n");
            return -1;
        }
    }
    else
    {
        printf("\nVentoy is not installed.\n");
        printf("Installing Ventoy...\n");

        if (installVentoy(dev) != 0)
        {
            fprintf(stderr, "Ventoy installation failed\n");
            return -1;
        }

        char *settle_cmd[] = {
            "udevadm",
            "settle",
            NULL
        };

        if (run_checked(settle_cmd) != 0)
            return -1;

        ventoy_status = detectVentoyDataPartition(dev);

        if (ventoy_status != 1)
        {
            fprintf(stderr,
                    "Ventoy data partition not found after installation\n");
            return -1;
        }
    }

    if (mountUSB(dev) != 0)
    {
        fprintf(stderr, "Cannot mount Ventoy partition\n");
        return -1;
    }

    if (deleteOldISOs() != 0)
    {
        unmountUSB();
        fprintf(stderr, "Failed to remove old ISO files.\n");
        return 0;
    }

    int space_ok = hasEnoughSpaceOnMountedUSB(iso);

    if (space_ok != 1)
    {
        if (unmountUSB() != 0)
            fprintf(stderr, "WARNING: USB partition remains mounted!\n");

        return -1;
    }

    int result = copyISO(iso);

    if (unmountUSB() != 0)
    {
        fprintf(stderr, "Warning: could not unmount USB partition\n");
        return -1;
    }

    return result;
}
