
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/statvfs.h>

#include "exec.h"
#include "usb.h"
#include "iso.h"
#include "utils.h"
#include "ventoy.h"
#include "devices.h"

#define MNT_USB_PATH "/mnt/grapeusb_usb"

static int readLsblk(const char *device, char *buffer, size_t size)
{
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0)
    {
        dup2(pipefd[1], STDOUT_FILENO);

        close(pipefd[0]);
        close(pipefd[1]);

        execlp("lsblk", "lsblk",
               "-nrpo", "NAME,TYPE,FSTYPE,LABEL,PKNAME,PARTN",
               device, (char *)NULL);

        perror("lsblk");
        _exit(127);
    }

    close(pipefd[1]);

    size_t total = 0;
    ssize_t n;

    while ((n = read(pipefd[0], buffer + total,
                     size - total - 1)) > 0)
    {
        total += (size_t)n;

        if (total >= size - 1)
        {
            fprintf(stderr, "lsblk output too large\n");
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            return -1;
        }
    }

    close(pipefd[0]);

    int status;
    if (waitpid(pid, &status, 0) == -1 ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        fprintf(stderr, "lsblk failed\n");
        return -1;
    }

    buffer[total] = '\0';
    return 0;
}


int detectVentoyDataPartition(UsbDevice *dev)
{
    if (dev == NULL)
        return -1;

    char output[4096];

    if (readLsblk(dev->dev_path, output, sizeof(output)) != 0)
        return -1;

    int ventoy_partition_found = 0;
    int efi_partition_found = 0;

    char data_partition[128] = "";

    char *line = strtok(output, "\n");

    while (line != NULL)
    {
        char path[128] = "";
        char type[32] = "";
        char fstype[32] = "";
        char label[64] = "";
        char parent[64] = "";
        int partition_number = 0;

        int fields = sscanf(
            line,
            "%127s %31s %31s %63s %63s %d",
            path, type, fstype, label, parent, &partition_number
        );

        if (fields == 6 &&
            strcmp(type, "part") == 0 &&
            strcmp(parent, dev->name) == 0)
        {
            if (strcmp(label, "Ventoy") == 0 &&
                strcmp(fstype, "exfat") == 0 &&
                partition_number == 1)
            {
                snprintf(data_partition, sizeof(data_partition),
                         "%s", path);

                ventoy_partition_found = 1;
            }

            if (strcmp(label, "VTOYEFI") == 0 &&
                partition_number == 2)
            {
                efi_partition_found = 1;
            }
        }

        line = strtok(NULL, "\n");
    }

    if (ventoy_partition_found && efi_partition_found)
    {
        snprintf(dev->part_path, sizeof(dev->part_path),
                 "%s", data_partition);

        printf("Ventoy installation detected: %s\n",
               dev->part_path);

        return 1;
    }

    dev->part_path[0] = '\0';

    return 0;
}

int mountUSB(UsbDevice *dev)
{
    if (dev == NULL || dev->part_path[0] == '\0')
    {
        fprintf(stderr, "Invalid USB partition\n");
        return -1;
    }

    if (mkdir(MNT_USB_PATH, 0755) != 0 && access(MNT_USB_PATH, F_OK) != 0)
    {
        perror("Cannot create USB mount directory");
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

int unmountUSB(void)
{
    char *cmd[] = {
        "umount",
        MNT_USB_PATH,
        NULL
    };

    return run_checked(cmd);
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

    int space_ok = hasEnoughSpaceOnMountedUSB(iso);

    if (space_ok != 1)
    {
        unmountUSB();
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
