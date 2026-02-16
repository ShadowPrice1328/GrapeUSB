#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include "jsmn.h"

#define CMD_COUNT (sizeof(cmds) / sizeof(cmds[0]))
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
} UsbDevice;

typedef enum {
    ISO_UNKNOWN,
    ISO_WINDOWS,
    ISO_LINUX
} IsoType;

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

int unmountUSB(UsbDevice *dev_data)
{
    if (!findUsbByName(dev_data->name, dev_data))
    {
        fprintf(stderr, "Error: USB device %s not found.\n", dev_data->name);
        return -1;
    }
    
    char device_path[128];
    snprintf(device_path, sizeof(device_path), "/dev/%s", dev_data->name);


    char *unmount_cmd[] = {"umount", "-f", device_path, NULL}; 
    
    int result = run(unmount_cmd);
    
    if (result != 0) {
        printf("Notice: Could not unmount %s, it might already be unmounted.\n", device_path);
    }

    return result;
}

int formatUSB(UsbDevice *dev_data)
{
    if (!findUsbByName(dev_data->name, dev_data))
    {
        fprintf(stderr, "Error: USB device %s not found.\n", dev_data->name);
        return -1;
    }

    char part[64];

    if (snprintf(part, sizeof(part), "%s", dev_data->name) >= sizeof(part))
    {
        fprintf(stderr, "Device path too long\n");
        return -1;
    }

    char *cmds[][9] = 
    {
        {"wipefs", "-a", (char*)dev_data->name, NULL},
        {"parted", (char*)dev_data->name, "--script", "mklabel", "gpt", NULL},
        {"parted", (char*)dev_data->name, "--script", "mkpart", "primary", "fat32", "4MiB", "100%", NULL},
        {"parted", "-s", (char*)dev_data->name, "set", "1", "esp", "on", NULL},
        {"udevadm", "settle", NULL},
        {"mkfs.vfat", "-F32", part, NULL}
    };

    for (int i = 0; i < CMD_COUNT; i++)
    {
        if (run(cmds[i]) != 0)        
        {
            fprintf(stderr, "Failed to execute command in formatUSB: %s\n", cmds[i][0]);
            return -1;
        }
    }

    return 0;
}

int mountISO(const char *iso)
{
    
    if (access(MNT_ISO_PATH, F_OK) != 0)
    {
        if (mkdir(MNT_ISO_PATH, 0755) != 0)
        {
            perror("Failed to create mount point while mounting ISO");
            return -1;
        }
    }
    
    char *mount[] = {"mount", "-o", "loop,ro", (char*)iso, MNT_ISO_PATH, NULL};

    int res = run(mount);
    if (res != 0)
        fprintf(stderr, "Failed to mount ISO: %s\n", iso);
    
    return res;
}

void unmountISO()
{
    char *umount_cmd[] = {"umount", "/mnt/iso", NULL};
    run(umount_cmd);
}

int isWindowsISO()
{
    return access("/mnt/iso/sources/install.wim", F_OK) == 0 ||
           access("/mnt/iso/sources/install.esd", F_OK) == 0;
}

int isLinuxISO()
{
    return access("/mnt/iso/casper", F_OK) == 0 ||
           access("/mnt/iso/isolinux", F_OK) == 0 ||
           access("/mnt/iso/boot", F_OK) == 0;
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
    else unmountUSB(dev_data);

    char part_path[128];

    if (strncmp(dev_data->name, "/dev/nvme", 4) == 0)
        snprintf(part_path, sizeof(part_path), "%sp1", dev_data->name);
    else
        snprintf(part_path, sizeof(part_path), "%s1", dev_data->name);

    char *mount[] = {"mount", "-o", "rw,flush", part_path, MNT_USB_PATH, NULL};
    
    int res = run(mount);
    if (res != 0)
    {
        fprintf(stderr, "Failed to mount USB to %s\n", part_path);
        return -1;
    }

    return 0;
}

void splitWimIfNeeded() {
    char *split[] = {
        "wimlib-imagex", "split",
        MNT_ISO_PATH "/sources/install.wim",
        MNT_USB_PATH "/sources/install.swm",
        "3800", NULL
    };
    run(split);
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
            "--exclude", "sources/install.wim", 
            "--exclude", "sources/install.esd", 
            MNT_ISO_PATH "/", MNT_USB_PATH "/", NULL
        };

        if (run(copy_base) != 0) 
            return -1;

        char wim_path[256];
        snprintf(wim_path, sizeof(wim_path), "%s/souces/install.wim", MNT_ISO_PATH);

        if (access(wim_path, F_OK) == 0)
        {
            struct stat st;
            stat(wim_path, &st);
            if (st.st_size > 4294967295LL)
            {
                splitWimIfNeeded(); 
            }
            else
            {
                char *copy_wim[] = {"cp", wim_path, MNT_USB_PATH "/sources/", NULL};
                run(copy_wim);
            }
        }
    }
    else
    {
        char *copy_linux[] = {"rsync", "-ah", "--progress", MNT_ISO_PATH "/", MNT_USB_PATH "/", NULL};
        if (run(copy_linux) != 0) return -1;
    }

    system("sync");

    return 0;
}

void cleanup() {
    char *u1[] = {"umount", "/mnt/iso", NULL};
    char *u2[] = {"umount", "/mnt/usb", NULL};

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
    #ifdef _WIN32
        system("cls");
    #else 
        system("clear");
    #endif
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
                else if (strncmp(mountpoint, "mountpoint", key_len) == 0)
                    snprintf(mountpoint, sizeof(mountpoint), "%.*s", val_len, val_ptr);

                j += 2; 
            }

            int is_system = (strcmp(mountpoint, "/") == 0 || 
                            strcmp(mountpoint, "/boot") == 0);

            

            if (strcmp(type, "disk") == 0 && rm == 1 && !is_system) 
            {
                strncpy(list[count].name, name, sizeof(list[count].name));
                strncpy(list[count].size, size, sizeof(list[count].size));
                strncpy(list[count].model, model, sizeof(list[count].model));
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

    printf("Selected flashdrive: %s (%s, %s)\n\n", dev_data->name, dev_data->size, dev_data->model);

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

void create_bootable(const char *iso, UsbDevice *dev) {
    unmountUSB(dev);
    formatUSB(dev);

    mountISO(iso);
    mountUSB(dev);

    copyFiles();
    //splitWimIfNeeded(); TO DO: перевірка коли треба юзати

    cleanup();
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

    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].name, name) == 0) {
            *devOut = list[i];
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

    getchar();
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
                if (dev_data.name[0] == '\0') {
                    printf("No USB device selected! Press Enter...\n");
                    getchar();
                    flushInput();
                    current = DEVICES;
                    break;
                }

                // create_bootable(argv[1], dev_data);
                // printf("\nBootable USB creation complete! Press Enter...\n");
                // getchar();
                // flushInput();
                // current = MENU;
                break;
            default:
                current = EXIT;
        }
    }

    clearScreen();
    printf("Thank you, goodbye =)\n");
    
    return 0;

    // TO DO: усі можливі застереження, дебілостійкість, mqueue,є
    //  роллбек після кожного етапу
}
