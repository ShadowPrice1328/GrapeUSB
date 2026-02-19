#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/statvfs.h>
#include "jsmn.h"

#define MNT_ISO_PATH "/mnt/iso"
#define MNT_USB_PATH "/mnt/usb"

typedef enum {
    MENU,
    MAIN_INFO,
    DEVICES,
    BEGIN,
    START,
    EXIT
} Screen;

typedef struct {
    char name[64];
    char size[16];
    char model[128];
    char dev_path[64];
    char part_path[64];
} UsbDevice;

typedef enum {
    ISO_UNKNOWN,
    ISO_WINDOWS,
    ISO_LINUX
} IsoType;

void printTime();
void flushInput();
int findUsbByName(const char *name, UsbDevice *devOut);

int run(char *const argv[])
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(argv[0], argv);
        perror("execvp failed");
        exit(127);
    }
    else if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid failed");
            return -1;
        }

        if (WIFEXITED(status))
        {
            int exitCode = WEXITSTATUS(status);
            if (exitCode != 0)
                fprintf(stderr, "Command failed with exit code %d: %s\n", exitCode, argv[0]);

            return exitCode;
        }
        else if (WIFSIGNALED(status))
        {
            fprintf(stderr, "Command killed by signal %d: %s\n", WTERMSIG(status), argv[0]);
            return -1;
        }

        return -1;
    }
    else
    {
        perror("fork failed");
        return -1;
    }
}

int run_checked(char *const argv[])
{
    int res = run(argv);

    if (res != 0)
        fprintf(stderr, "Failed: command failed: %s\n", argv[0]);

    return res;
}

int hasEnoughSpace(const char *isoPath, UsbDevice *dev) 
{
    struct stat st;

    if (stat(isoPath, &st) != 0) 
    {
        perror("stat ISO failed");
        return 0;
    }

    long long isoSize = st.st_size;
    
    char sysfs_path[256];
    snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/block/%s/size", dev->name);
    
    FILE *f = fopen(sysfs_path, "r");
    
    if (!f)
    {
        perror("Failed to read device size");
        return 0;
    }

    long long blocks;
    fscanf(f, "%lld", &blocks);
    fclose(f);

    long long devSize = blocks * 512;

    printf("ISO size: %lld bytes, USB size: %lld bytes\n", isoSize, devSize);

    return devSize > isoSize;
}

int unmountUSB(UsbDevice *dev_data)
{
    char *unmount_cmd[] = {"umount", "-f", dev_data->dev_path, NULL}; 
    
    int result = run(unmount_cmd);
    
    if (result != 0)
        printf("Notice: Could not unmount %s, it might already be unmounted.\n", dev_data->dev_path);

    return result;
}

int formatUSB(UsbDevice *dev_data)
{
    char *cmds[][9] = 
    {
        {"wipefs", "-a", dev_data->dev_path, NULL},
        {"parted", dev_data->dev_path, "--script", "mklabel", "gpt", NULL},
        {"parted", dev_data->dev_path, "--script", "mkpart", "primary", "fat32", "4MiB", "100%", NULL},
        {"parted", "-s", dev_data->dev_path, "set", "1", "esp", "on", NULL},
        {"udevadm", "settle", NULL},
        {"mkfs.vfat", "-F32", dev_data->part_path, NULL}
    };

    size_t cmd_count = sizeof(cmds) / sizeof(cmds[0]);

    for (int i = 0; i < cmd_count; i++)
    {
        if (run_checked(cmds[i]) != 0)
            return -1;
    }

    return 0;
}

void unmountISO()
{
    char *umount_cmd[] = {"umount", MNT_ISO_PATH, NULL};
    run(umount_cmd);
}

int mountISO(const char *iso)
{
    char *umount_cmd[] = {"umount", "-l", MNT_ISO_PATH, NULL};
    run(umount_cmd);

    if (access(MNT_ISO_PATH, F_OK) != 0)
    {
        if (mkdir(MNT_ISO_PATH, 0755) != 0)
        {
            perror("Failed to create mount point while mounting ISO");
            return -1;
        }
    }
    
    char *mount[] = {"mount", "-o", "loop,ro", (char*)iso, MNT_ISO_PATH, NULL};

    if (run_checked(mount) != 0)
        return -1;
    
    return 0;
}


int isWindowsISO()
{
    return access(MNT_ISO_PATH "/sources/install.wim", F_OK) == 0 ||
           access(MNT_ISO_PATH "/sources/install.esd", F_OK) == 0;
}

int isLinuxISO()
{
    return access(MNT_ISO_PATH "/casper", F_OK) == 0 ||    // Ubuntu/Mint
           access(MNT_ISO_PATH "/live", F_OK) == 0 ||      // Debian/Kali
           access(MNT_ISO_PATH "/LiveOS", F_OK) == 0 ||    // Fedora/CentOS/RedHat
           access(MNT_ISO_PATH "/images", F_OK) == 0 ||    // Fedora
           access(MNT_ISO_PATH "/arch", F_OK) == 0  ||     // Arch
           access(MNT_ISO_PATH "/isolinux", F_OK) == 0;    // Old ones
}

IsoType detectISOType(const char* iso)
{
    if (mountISO(iso) != 0)
        return ISO_UNKNOWN;

    IsoType type = ISO_UNKNOWN;

    if (isWindowsISO())
        type = ISO_WINDOWS;
    else if (isLinuxISO())
        type = ISO_LINUX;
    
    unmountISO();
    return type;
}

int mountUSB(UsbDevice *dev_data)
{
    if (access(MNT_USB_PATH, F_OK) != 0)
    {
        if (mkdir(MNT_USB_PATH, 0755) != 0)
        {
            perror("Failed to create mount point");
            return -1;
        }
    }

    char *mount[] = {"mount", "-o", "rw,flush", dev_data->part_path, MNT_USB_PATH, NULL};
    
    if (run_checked(mount) != 0)
        return -1;

    return 0;
}

int splitWimIfNeeded() 
{
    char *split[] = 
    {
        "wimlib-imagex", "split",
        MNT_ISO_PATH "/sources/install.wim",
        MNT_USB_PATH "/sources/install.swm",
        "3800", NULL
    };

    return run_checked(split);
}

int copyFiles(IsoType type) 
{
    if (access(MNT_ISO_PATH, R_OK) != 0)
    {
        fprintf(stderr, "ISO not mounted or not readable\n");
        return -1;
    }

    if (access(MNT_USB_PATH, R_OK) != 0)
    {
        fprintf(stderr, "USB not mounted or not readable\n");
        return -1;
    }

    if (type == ISO_WINDOWS)
    {
        char *copy_base[] = {
            "rsync", "-ah", "--progress", 
            "--no-perms", "--no-owner", "--no-group",
            "--exclude", "sources/install.wim", 
            "--exclude", "sources/install.esd", 
            MNT_ISO_PATH "/", MNT_USB_PATH "/", NULL
        };

        if (run_checked(copy_base) != 0)
            return -1;

        char wim_path[256];
        snprintf(wim_path, sizeof(wim_path), "%s/sources/install.wim", MNT_ISO_PATH);

        if (access(wim_path, F_OK) == 0)
        {
            struct stat st;
            stat(wim_path, &st);
            if (st.st_size > 4294967295LL)
            {
                if (splitWimIfNeeded() != 0)
                    return -1;
            }
            else
            {
                char *copy_wim[] = {"cp", wim_path, MNT_USB_PATH "/sources/", NULL};

                if (run_checked(copy_wim) != 0)
                    return -1;
            }
        }
    }
    else
    {
        char *copy_linux[] = {"rsync", "-ah", "--progress", MNT_ISO_PATH "/", MNT_USB_PATH "/", NULL};
        
        if (run_checked(copy_linux) != 0)
            return -1;

        return 0;
    }

    char *sync_cmd[] = {"sync", NULL};
    
    if (run_checked(sync_cmd) != 0)
        return -1;

    return 0;
}

void cleanup() 
{
    char *u1[] = {"umount", MNT_ISO_PATH, NULL};
    char *u2[] = {"umount", MNT_USB_PATH, NULL};

    run(u1);
    run(u2);
}

void checkRoot()
{
    if (geteuid() != 0)
    {
        printf("Run this program as root (sudo).\n");
        exit(1);
    }
}

void clearScreen()
{
    printf("\033c");
}

int getUsbDevices(UsbDevice *list, int max)
{
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        return 0;
    }

    pid_t pid = fork();

    if (pid == 0) // child process goes to lsblk
    { 
        dup2(pipefd[1], STDOUT_FILENO); // now lsblk goes from console to pipe
        close(pipefd[0]);
        close(pipefd[1]);

        execlp("lsblk", "lsblk", "-J", "-d", "-o",
               "NAME,RM,SIZE,MODEL,TYPE,MOUNTPOINT", NULL);

        perror("execlp");
        exit(1);
    }

    close(pipefd[1]);

    char buffer[8192];
    ssize_t len = read(pipefd[0], buffer, sizeof(buffer) - 1);
    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    if (len <= 0)
        return 0;

    buffer[len] = '\0';

    jsmn_parser parser;
    jsmntok_t tokens[256];

    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, buffer, len, tokens, 256);

    if (token_count < 0)
        return 0;

    int count = 0;

    for (int i = 0; i < token_count && count < max; i++) 
    {
        if (tokens[i].type == JSMN_OBJECT) 
        {
            char name[64] = "", size[32] = "", model[128] = "", type[32] = "", mountpoint[256] = "";
            int rm = 0;
            
            int obj_size = tokens[i].size; 
            int j = i + 1;

            for (int k = 0; k < obj_size; k++) 
            {
                int key_len = tokens[j].end - tokens[j].start;
                char *key = &buffer[tokens[j].start];

                jsmntok_t *val = &tokens[j + 1];
                int val_len = val->end - val->start;
                char *val_ptr = &buffer[val->start];

                if (strncmp(key, "name", key_len) == 0)
                    snprintf(name, sizeof(name), "%.*s", val_len, val_ptr);
                else if (strncmp(key, "size", key_len) == 0)
                    snprintf(size, sizeof(size), "%.*s", val_len, val_ptr);
                else if (strncmp(key, "model", key_len) == 0)
                    snprintf(model, sizeof(model), "%.*s", val_len, val_ptr);
                else if (strncmp(key, "type", key_len) == 0)
                    snprintf(type, sizeof(type), "%.*s", val_len, val_ptr);
                else if (strncmp(key, "rm", key_len) == 0) 
                {
                    if (strncmp(val_ptr, "true", 4) == 0 || strncmp(val_ptr, "1", 1) == 0)
                        rm = 1;
                    else
                        rm = 0;
                }
                else if (strncmp(key, "mountpoint", key_len) == 0)
                    snprintf(mountpoint, sizeof(mountpoint), "%.*s", val_len, val_ptr);

                j += 2; 
            }

            int is_system = (strcmp(mountpoint, "/") == 0 || 
                            strcmp(mountpoint, "/boot") == 0);

            

            if (strcmp(type, "disk") == 0 && rm == 1 && !is_system) 
            {
                snprintf(list[count].name, sizeof(list[count].name), "%s", name);
                snprintf(list[count].size, sizeof(list[count].size), "%s", size);
                snprintf(list[count].model, sizeof(list[count].model), "%s", model);
                count++;
            }
            
            i = j - 1;
        }
    }

    return count;
}

int getCharInput()
{
    int c = getchar();
    flushInput();
    return c;
}

Screen showDevices(UsbDevice *dev_data) 
{
    clearScreen();

    printf("\033[47;30m  ★ DEVICES ★  \n\033[0m\n");

    UsbDevice devs[16];
    int n = getUsbDevices(devs, 16);

    if (n == 0) 
    {
        printf("No USB flash drives detected.\n\n");
        printf(" [Z] Back to Menu\n");
        printf("\nEnter choice: ");

        int input = getCharInput();
        if (input == 'z' || input == 'Z') return MENU;
        return DEVICES;
    }

    printf("Detected USB flash drives:\n\n");

    for (int i = 0; i < n; i++) 
    {
        if (dev_data->name[0] != '\0' && strcmp(devs[i].name, dev_data->name) == 0) 
        {
            printf("\033[47;30m [%d] %-10s %-8s %s\033[0m\n", 
                    i + 1, devs[i].name, devs[i].size, devs[i].model);
        } 
        else 
        {
            printf(" [%d] %-10s %-8s %s\n",
                   i + 1, devs[i].name, devs[i].size, devs[i].model);
        }
    }

    printf("\n [1-%d] Select device\n [Z] Back to Menu\n\nEnter choice: ", n);
    int input = getCharInput();

    if (input >= '1' && input <= '0' + n) 
    {
        int idx = input - '1';
        *dev_data = devs[idx];

        char full_path[128];
        snprintf(full_path, sizeof(full_path), "/dev/%s", dev_data->name);

        strncpy(dev_data->dev_path, full_path, sizeof(dev_data->dev_path));

        if (strstr(dev_data->name, "nvme") || strstr(dev_data->name, "mmcblk"))
            snprintf(dev_data->part_path, sizeof(dev_data->part_path), "%sp1", full_path);
        else
            snprintf(dev_data->part_path, sizeof(dev_data->part_path), "%s1", full_path);

        return DEVICES;
    }

    if (input == 'z' || input == 'Z') 
        return MENU;

    return DEVICES;
}


Screen showBeginCreation(UsbDevice *dev_data, IsoType isoType) 
{
    clearScreen();

    printf("\033[47;30m  ★ CREATE BOOTABLE USB FLASHDRIVE ★  \n\n\033[0m");

    const char* isoStr = (isoType == ISO_WINDOWS) ? "Windows" : "Linux";
    printf("ISO Type: %s\n", isoStr);

    if (dev_data == NULL || dev_data->name[0] == '\0')
    {
        printf("No USB device selected!\n\n");
        printf("Please select a USB drive first.\n\n");

        printf(" [1] Show available devices\n");
        printf(" [Z] Back to Menu\n");

        printf("\nEnter choice: ");
        int input = getCharInput();

        if (input == '1') return DEVICES;
        if (input == 'z' || input == 'Z') return MENU;

        return BEGIN;
    }

    if (isoType == ISO_UNKNOWN)
    {
        printf("No valid ISO detected!\n");
        printf("Please provide a valid Windows or Linux ISO.\n\n");
        printf(" [Z] Back to Menu\n");

        printf("\nEnter choice: ");
        int input = getCharInput();

        if (input == 'z' || input == 'Z') return MENU;

        return BEGIN;
    }

    printf("Selected flashdrive: %s (%s, %s)\n\n", dev_data->dev_path, dev_data->size, dev_data->model);

    printf("Is it correct? [Y/N]: ");
    int input = getCharInput();

    if (input == 'y' || input == 'Y')
        return START;
    if (input == 'n' || input == 'N')
        return DEVICES;

    return BEGIN;
}

int fileExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int isValidISO(const char *iso)
{
    char *blkid[] = {"blkid", "-o", "value", "-s", "TYPE", (char*)iso, NULL};
    return run(blkid) == 0;
}

int create_bootable(const char *iso, UsbDevice *dev, IsoType isoType) 
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

Screen showStartCreation(UsbDevice *dev_data, const char* iso, IsoType isoType)
{
    if (!hasEnoughSpace(iso, dev_data)) 
    {
        printf("\033[1;31mError: Not enough space on %s!\033[0m\n", dev_data->dev_path);
        printf("Press Enter to go back...");
        getchar();

        return DEVICES;
    }

    clearScreen();
    printf("\033[1;31m!!! WARNING: ALL DATA ON %s WILL BE ERASED !!!\033[0m\n\n", dev_data->dev_path);

    const char* isoStr = (isoType == ISO_WINDOWS) ? "Windows" : "Linux";

    printf("Selected ISO: %s\n", iso);
    printf("ISO type: %s\n", isoStr);
    printf("Selected flashdrive: %s (%s, %s)\n\n", dev_data->dev_path, dev_data->size, dev_data->model);

    printf("Are you absolutely sure? [Y/N]: ");
    
    int input = getCharInput();
    
    if (input == 'y' || input == 'Y') 
    {
        printTime();
        printf("\n>>> Starting the process. This may take a while...\n");

        if (create_bootable(iso, dev_data, isoType) != 0)
        {
            printf("\n\033[1;31mError occurred during creation!\033[0m\n");
            printf("Press Enter to return...");
            flushInput();
            return MENU;
        }

        printTime();
        printf("\n\033[1;32m★ Success! Bootable USB created. ★\033[0m\n");
        printf("Press Enter to return to menu...");
        getchar();

        return MENU;
    } 
    else 
    {
        printf("Operation cancelled.\n");
        sleep(1);
        return MENU;
    }
}

void flushInput() 
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}


void printTime() 
{
    time_t t = time(NULL);
    struct tm lcltm = *localtime(&t);
    
    printf("\n┌───────────────────────────┐\n");
    printf("│  %d-%02d-%02d %02d:%02d:%02d      │\n", 
        lcltm.tm_year + 1900, lcltm.tm_mon + 1, lcltm.tm_mday,
        lcltm.tm_hour, lcltm.tm_min, lcltm.tm_sec);
    printf("└───────────────────────────┘\n");
}

int findUsbByName(const char *name, UsbDevice *devOut) {
    UsbDevice list[16];
    int n = getUsbDevices(list, 16);
    

    for (int i = 0; i < n; i++) 
    {
        char full_path[128];
        snprintf(full_path, sizeof(full_path), "/dev/%s", list[i].name);

        if (strcmp(list[i].name, name) == 0 || strcmp(full_path, name) == 0)
        {
            *devOut = list[i];

            strncpy(devOut->dev_path, full_path, sizeof(devOut->dev_path));

            if (strstr(list[i].name, "nvme") || strstr(list[i].name, "mmcblk"))
                snprintf(devOut->part_path, sizeof(devOut->part_path), "%sp1", full_path);
            else
                snprintf(devOut->part_path, sizeof(devOut->part_path), "%s1", full_path);

            return 1;           
        }
    }
    return 0;
}

Screen showMenu()
{
    clearScreen();

    printf("Choose one from the options below:\n\n");
    printf(" [1] About\n");
    printf(" [2] Show available devices\n");
    printf(" [3] Begin\n");
    printf(" [Z] Exit\n");
    printf("\nEnter choice: ");

    int input = getCharInput();

    if (input == '1') return MAIN_INFO;
    if (input == 'z' || input == 'Z') return EXIT;
    if (input == '2') return DEVICES;
    if (input == '3') return BEGIN;

    printf("\nWrong option. Press Enter to continue...");
    flushInput();

    return MENU;
}

Screen showMainInfo() 
{
    clearScreen();

    printf("\033[47;30m");
    printf("  ★ ABOUT ★  \n");
    printf("\033[0m\n");

    printf("===================================================\n");
    printf(" Welcome to the \"GrapeUSB\" Utility!\n");
    printf("===================================================\n\n");

    printf("This utility helps you with creation of a bootable USB flashdrive from a .iso file.\n\n");
    printf(" [Z] Back to Menu\n");
    printf("\nEnter choice: ");

    int input = getCharInput();

    if (input == 'z' || input == 'Z')
        return MENU;

    return MAIN_INFO;
}

int validateISOArgument(const char* iso, IsoType* type)
{
    if (!fileExists(iso)) {
        fprintf(stderr, "ISO file does not exist: %s\n", iso);
        return 1;
    }

    if (!isValidISO(iso))
    {
        fprintf(stderr, "ISO file is not valid: %s\n", iso);
        return 1;
    }

    *type = detectISOType(iso);
    if (*type == ISO_UNKNOWN)
    {
        fprintf(stderr, "ISO file type is unsupported: %s\n", iso);
        return 1;
    }

    if (*type == ISO_WINDOWS)
        printf("Detected Windows ISO\n");

    if (*type == ISO_LINUX)
        printf("Detected Linux ISO\n");

    return 0;
}

int main(int argc, char* argv[]) 
{
    checkRoot();

    if (argc != 3)
    {
        printf("Usage: %s path/to/.iso /dev/sdX (or \"0\" if not known)\n", argv[0]);
        return 1;
    }

    IsoType isoType = ISO_UNKNOWN;

    if (validateISOArgument(argv[1], &isoType) == 1)
        return 1;

    UsbDevice dev_data = {0};
    char *dev = NULL;

    if (strcmp(argv[2], "0") != 0) 
    {
        dev = argv[2];

        if (!findUsbByName(dev, &dev_data))
            dev = NULL;
    }

    Screen current = MENU;

    while (current != EXIT)
    {
        switch (current)
        {
            case MENU:
                current = showMenu();
                break;
            case MAIN_INFO:
                current = showMainInfo();
                break;
            case DEVICES:
                current = showDevices(&dev_data);
                break;
            case BEGIN:
                current = showBeginCreation(&dev_data, isoType);
                break;
            case START:
                if (dev_data.name[0] == '\0') 
                {
                    current = DEVICES;
                    break;
                }
                current = showStartCreation(&dev_data, argv[1], isoType);
                break;
            default:
                current = EXIT;
        }
    }

    clearScreen();
    printf("Thank you, goodbye =)\n");
    
    return 0;

    // TO DO: mqueue
    //  роллбек після кожного етапу
}
