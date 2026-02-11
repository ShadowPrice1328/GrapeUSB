#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

typedef enum {
    MENU,
    MAIN_INFO,
    EXIT
} Screen;

int run(char *const argv[])
{
    pid_t pid = fork();

    if (pid == 0)
    {
        perror(argv[0]);
    }
    else if (pid > 0)
    {
        perror("fork");
        return -1;
    }   
    
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    
    return -1;
}

void unmountUSB(const char *dev)
{
    char part[64];
    snprintf(part, sizeof(part), "%s1", dev);

    char *unmount[] = {"unmount", part, NULL};
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

void mountISO(const char *iso)
{
    mkdir("/mnt/iso", 0755);
    
    char *mount[] = {"mount", (char*)iso, "/mnt/iso", NULL};
    run(mount);
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

void create_bootable(const char *iso, const char *dev) {
    checkRoot();

    unmountUSB(dev);
    formatUSB(dev);

    mountISO(iso);
    mountUSB(dev);

    copyFiles();
    splitWimIfNeeded();

    cleanup();
}

void flushInput() 
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void clearScreen()
{
    #ifdef _WIN32
        system("cls");
    #else 
        system("clear");
    #endif
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

Screen showMenu()
{
    clearScreen();

    system("clear");
    printf("Choose one from the options below:\n");
    printf(" [1] Main information\n");
    printf(" [0] Exit\n");
    printf("\nEnter choice: ");

    int input = getchar();
    flushInput();

    if (input == '1') return MAIN_INFO;
    if (input == '0') return EXIT;

    printf("\nWrong option. Press Enter to continue...");
    getchar();

    return MENU;
}

Screen showMainInfo() 
{
    clearScreen();

    printf("\033[47;30m");
    printf("  ★ MAIN INFO ★  \n");
    printf("\033[0m");

    printf("===================================================\n");
    printf(" Welcome to the \"Bootable Flashdrive\" Utility!\n");
    printf("===================================================\n\n");

    printf("This utility helps you with creation of a bootable USB flashdrive from a .iso file.\n\n");
    printf(" [Z] Back to Menu\n");
    printf("Enter choice: ");

    int input = getchar();
    flushInput();

    if (input == 'z' || input == 'Z')
        return MENU;

    return MAIN_INFO;
}

int main(int argc, char* argv[]) 
{
    checkRoot();

    if (argc != 3)
    {
        printf("Usage: %s windows.iso /dev/sdX\n", argv[0]);
        return 1;
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
            default:
                current = EXIT;
        }
    }

    clearScreen();
    printf("Thank you, goodbye\n");
    
    return 0;
}
