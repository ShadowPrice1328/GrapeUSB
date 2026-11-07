
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
#include <dirent.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdint.h>

#include "exec.h"
#include "usb.h"
#include "iso.h"
#include "ventoy.h"
#include "devices.h"
#include "utils.h"

#define MNT_USB_PATH "/mnt/grapeusb_usb"

static int deleteOldISOs(void)
{
    DIR *dir = opendir(MNT_USB_PATH);

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

        int len = snprintf(path, sizeof(path),
                           "%s/%s", MNT_USB_PATH, entry->d_name);

        if (len < 0 || (size_t)len >= sizeof(path))
            continue;

        struct stat st;

        if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        if (unlink(path) != 0)
        {
            perror("Cannot delete ISO");
            closedir(dir);
            return -1;
        }

        printf("Deleted: %s\n", entry->d_name);
        deleted++;
    }

    closedir(dir);

    printf("Deleted %d ISO file(s).\n", deleted);

    return 0;
}

static unsigned long long getAvailableSpace(void)
{
    struct statvfs fs;

    if (statvfs(MNT_USB_PATH, &fs) != 0)
    {
        perror("statvfs");
        return 0;
    }

    return (unsigned long long)fs.f_bavail * fs.f_frsize;
}

static int prepareVentoySpace(const char *iso_path)
{
    struct stat iso_stat;

    if (stat(iso_path, &iso_stat) != 0 ||
        !S_ISREG(iso_stat.st_mode))
    {
        fprintf(stderr, "Cannot determine ISO size.\n");
        return 0;
    }

    unsigned long long required = iso_stat.st_size;

    DIR *dir = opendir("/mnt/grapeusb_usb");

    if (dir == NULL)
    {
        perror("Cannot open Ventoy partition");
        return 0;
    }

    struct dirent *entry;
    int iso_count = 0;

    printf("\nExisting ISO images:\n");

    while ((entry = readdir(dir)) != NULL)
    {
        const char *ext = strrchr(entry->d_name, '.');

        if (ext == NULL || strcasecmp(ext, ".iso") != 0)
            continue;

        char path[PATH_MAX];

        int len = snprintf(path, sizeof(path),
                           MNT_USB_PATH "/%s", entry->d_name);

        if (len < 0 || (size_t)len >= sizeof(path))
            continue;

        struct stat st;

        if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        printf("  - %s (%.2f GB)\n",
               entry->d_name,
               (double)st.st_size / 1073741824.0);

        iso_count++;
    }

    closedir(dir);

    unsigned long long available = getAvailableSpace();

    printf("\nNew ISO size: %.2f GB\n",
           (double)required / 1073741824.0);

    printf("Available space: %.2f GB\n",
           (double)available / 1073741824.0);

    if (available >= required)
    {
        if (iso_count == 0)
            return 1;

        printf("\nThere is enough space for the new ISO.\n");

        printf("1. Add new ISO and keep existing images.\n");
        printf("2. Delete all existing ISO images and replace them.\n");

        char choice[16];

        printf("Choose [1/2]: ");
        fflush(stdout);

        if (fgets(choice, sizeof(choice), stdin) == NULL)
            return 0;

        if (choice[0] == '1')
            return 1;

        if (choice[0] == '2')
        {
            if (!askUser("Delete ALL existing ISO images?"))
                return 0;

            if (deleteOldISOs() != 0)
                return 0;

            return 1;
        }

        printf("Invalid choice.\n");
        return 0;
    }

    printf("\nNot enough space for the new ISO.\n");

    if (iso_count == 0)
    {
        printf("No existing ISO images can be deleted.\n");
        return 0;
    }

    printf("All existing ISO images must be deleted to continue.\n");

    if (!askUser("Delete ALL existing ISO images and continue?"))
    {
        printf("Operation cancelled.\n");
        return 0;
    }

    if (deleteOldISOs() != 0)
        return 0;

    available = getAvailableSpace();

    if (available < required)
    {
        printf("Still not enough space after deleting old images.\n");
        return 0;
    }

    return 1;
}

int unmountDevicePartitions(const char *device)
{
    char command[256];

    snprintf(command, sizeof(command),
             "lsblk -nrpo MOUNTPOINT %s", device);

    FILE *fp = popen(command, "r");
    if (!fp)
        return -1;

    char line[512];
    char mounts[9][512];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < 16)
    {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] != '\0')
        {
            size_t len = strlen(line);

            if (len >= sizeof(mounts[0]))
            {
                fprintf(stderr, "Mount path is too long.\n");
                pclose(fp);
                return -1;
            }

            memcpy(mounts[count], line, len + 1);
            count++;
        }
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

    if (!prepareVentoySpace(iso))
    {
        unmountUSB();
        fprintf(stderr, "Operation cancelled or insufficient space.\n");
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
