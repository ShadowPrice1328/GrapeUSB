#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>

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

int run(char *const argv[])
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
    }
    else
    {
        perror("fork");
        return -1;
    }
}

void unmountUSB(const char *dev)
{
    char part[64];
    snprintf(part, sizeof(part), "%s1", dev);

    char *unmount[] = {"umount", part, NULL};
    run(unmount);
}

void formatUSB(const char *dev)
{
    char *wipe[] = {"wipefs", "-a", (char*)dev, NULL};
    run(wipe);

    char *label[] = {"parted", (char*)dev, "--script", "mklabel", "gpt", NULL};
    run(label);

    char *part[] = 
    {
        "parted", (char*)dev, "--script",
        "mkpart", "primary", "fat32", "4MiB", "100%",
        "set", "1", "esp", "on", NULL
    };
    run(part);

    char partdev[64];
    snprintf(partdev, sizeof(partdev), "%s1", dev);

    char *mkfs[] = {"mkfs.vfat", "-F32", partdev, NULL};
    run(mkfs);
}

int mountISO(const char *iso)
{
    mkdir("/mnt/iso", 0755);
    
    char *mount[] = {"mount", (char*)iso, "/mnt/iso", NULL};
    return run(mount);
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
    if (!mountISO(iso))
        return ISO_UNKNOWN;

    IsoType type = ISO_UNKNOWN;

    if (isWindowsISO())
        type = ISO_WINDOWS;
    else if (isLinuxISO())
        type = ISO_LINUX;
    
    unmountISO();
    return type;
}

void mountUSB(const char *dev)
{
    mkdir("/mnt/usb", 0755);

    char part[64];
    snprintf(part, sizeof(part), "%s1", dev);

    char *mount[] = {"mount", part, "/mnt/usb", NULL};
    run(mount);
}

void copyFiles() {
    char *copy[] = {"cp", "-rT", "/mnt/iso", "/mnt/usb", NULL};
    run(copy);
}

void splitWimIfNeeded() {
    char *split[] = {
        "wimlib-imagex", "split",
        "/mnt/iso/sources/install.wim",
        "/mnt/usb/sources/install.swm",
        "3800", NULL
    };
    run(split);
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
    pipe(pipefd); // pipe opening

    pid_t pid = fork();

    if (pid == 0) { // child process goes to lsblk
        dup2(pipefd[1], STDOUT_FILENO); // now lsblk goes from console to pipe
        close(pipefd[0]);
        close(pipefd[1]);

        execlp("lsblk", "lsblk", "-rpo",
               "NAME,RM,SIZE,MODEL,TYPE", NULL);

        perror("execlp");
        exit(1);
    }

    close(pipefd[1]);

    FILE *fp = fdopen(pipefd[0], "r");
    if (!fp) return 0;

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < max) 
    {
        char name[64], size[16], model[128], type[32];
        int rm;

        sscanf(line, "%63s %d %15s %127s %31s",
               name, &rm, size, model, type);

        if (rm == 1 && strcmp(type, "disk") == 0)  // TO DO: дуплікати disk/part забрати
        {
            strcpy(list[count].name, name);
            strcpy(list[count].size, size);
            strcpy(list[count].model, model);
            count++;
        }
    }

    fclose(fp);
    waitpid(pid, NULL, 0);

    return count;
}

Screen showDevices(UsbDevice *dev_data) 
{
    clearScreen();

    printf("\033[47;30m  ★ DEVICES ★  \n\033[0m");

    UsbDevice devs[16];
    int n = getUsbDevices(devs, 16);

    if (n == 0) 
    {
        printf("No USB flash drives detected.\n\n");
        printf(" [Z] Back to Menu\n");
        printf("\nEnter choice: ");
        int input = getchar();
        flushInput();
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
    int input = getchar();
    flushInput();

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


Screen showBeginCreation(UsbDevice *dev_data) 
{
    clearScreen();

    printf("\033[47;30m  ★ CREATE BOOTABLE USB FLASHDRIVE ★  \n\033[0m");

    if (dev_data->name[0] == '\0') 
    {
        printf("No USB device selected!\n");
        printf("Consider selecting it from the list.\n\n");

        printf(" [1] Show available devices\n");
        printf(" [Z] Back to Menu\n");

        printf("\nEnter choice: ");

        int input = getchar();
        flushInput();

        if (input == '1')
            return DEVICES;
        else if (input == 'z' || input == 'Z')
            return MENU;

        return BEGIN;
    }

    printf("Selected flashdrive: %s (%s, %s)\n\n",
           dev_data->name, dev_data->size, dev_data->model);
    printf("Is it correct? [Y/N]: ");

    int input = getchar();
    flushInput();

    if (input == 'y' || input == 'Y')
        return START;
    else if (input == 'n' || input == 'N')
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

void create_bootable(const char *iso, const char *dev) {
    checkRoot();

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

    printf("Choose one from the options below:\n");
    printf(" [1] About\n");
    printf(" [2] Show available devices\n");
    printf(" [3] Begin\n");
    printf(" [Z] Exit\n");
    printf("\nEnter choice: ");

    int input = getchar();
    flushInput();

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
    printf("\033[0m");

    printf("===================================================\n");
    printf(" Welcome to the \"GrapeUSB\" Utility!\n");
    printf("===================================================\n\n");

    printf("This utility helps you with creation of a bootable USB flashdrive from a .iso file.\n\n");
    printf(" [Z] Back to Menu\n");
    printf("\nEnter choice: ");

    int input = getchar();
    flushInput();

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
    if (type == ISO_UNKNOWN)
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

    IsoType isoType;

    if (!validateISOArgument(argv[1], &isoType))
        return 1;

    UsbDevice dev_data;
    char *dev = NULL;

    if (strcmp(argv[2], "0") != 0) {
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
                current = showBeginCreation(&dev_data);
                break;
            case START:
                //
                break;
            default:
                current = EXIT;
        }
    }

    clearScreen();
    printf("Thank you, goodbye =)\n");
    
    return 0;

    // TO DO: усі можливі застереження, для лінукса, дебілостійкість, mqueue,є
    //  роллбек після кожного етапу, lsblk може давати пробіли
}
