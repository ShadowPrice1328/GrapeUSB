#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "jsmn.h"
#include "utils.h"
#include "devices.h"

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
    ssize_t total = 0;
    ssize_t n = 0;

    while((n = read(pipefd[0], buffer + total, sizeof(buffer) - 1 - total)) > 0)
        total += n;

    if ((unsigned)total >= sizeof(buffer) - 1)
    {
        fprintf(stderr, "Output too large, buffer overflow\n");
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return 0;
    }

    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    if (total <= 0)
        return 0;

    buffer[total] = '\0';

    jsmn_parser parser;
    jsmntok_t tokens[256];

    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, buffer, total, tokens, 256);

    if (token_count < 0)
        return 0;

    int i;

    for (i = 1; i < token_count; i++)
    {
        if (tokens[i].type == JSMN_STRING &&
            (tokens[i].end - tokens[i].start) == 12 &&
            strncmp(buffer + tokens[i].start, "blockdevices", 12) == 0)
            {
                break;
            }
    }
    
    if (i == token_count)
        return 0;

    jsmntok_t *arr = &tokens[i + 1];

    if (arr->type != JSMN_ARRAY)
        return 0;

    int index = i + 2;
    int count = 0;

    for (int d = 0; d < arr->size && count < max; d++) 
    {
        jsmntok_t *obj = &tokens[index];
        
        if (obj->type != JSMN_OBJECT)
            break;
        
        char name[64] = "", size[32] = "", model[128] = "", type[32] = "", mountpoint[256] = "";
        int rm = 0;
        
        int obj_size = obj->size; 
        int j = index + 1;

        for (int k = 0; k < obj_size; k++) 
        {
            if (tokens[j].type != JSMN_STRING)
            {
                j += 2;
                continue;
            }

            int key_len = tokens[j].end - tokens[j].start;
            char *key = &buffer[tokens[j].start];

            jsmntok_t *val = &tokens[j + 1];
            int val_len = val->end - val->start;
            char *val_ptr = &buffer[val->start];

            if (key_len == 4 && strncmp(key, "name", key_len) == 0)
                snprintf(name, sizeof(name), "%.*s", val_len, val_ptr);
            else if (key_len == 4 && strncmp(key, "size", key_len) == 0)
                snprintf(size, sizeof(size), "%.*s", val_len, val_ptr);
            else if (key_len == 5 && strncmp(key, "model", key_len) == 0)
                snprintf(model, sizeof(model), "%.*s", val_len, val_ptr);
            else if (key_len == 4 && strncmp(key, "type", key_len) == 0)
                snprintf(type, sizeof(type), "%.*s", val_len, val_ptr);
            else if (key_len == 2 && strncmp(key, "rm", key_len) == 0) 
            {
                if (val_len == 4 && strncmp(val_ptr, "true", 4) == 0)
                    rm = 1;
                else if (val_len == 1 && strncmp(val_ptr, "1", 1) == 0)
                    rm = 1;
                else
                    rm = 0;
            }

            else if (key_len == 10 && strncmp(key, "mountpoint", key_len) == 0)
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
        
        index = j;
    }

    return count;
}

int findUsbByName(const char *name, UsbDevice *devOut) 
{
    UsbDevice list[16];
    int n = getUsbDevices(list, 16);

    for (int i = 0; i < n; i++) 
    {
        char full_path[128];
        snprintf(full_path, sizeof(full_path), "/dev/%s", list[i].name);

        if (strcmp(list[i].name, name) == 0 || strcmp(full_path, name) == 0)
        {
            *devOut = list[i];

            formatPartPath(devOut);
            return 1;           
        }
    }
    return 0;
}

static int readCommandOutput(const char *command, char *buffer, size_t size)
{
    FILE *fp = popen(command, "r");

    if (fp == NULL)
    {
        perror("popen");
        return -1;
    }

    size_t used = 0;

    while (fgets(buffer + used, (int)(size - used), fp) != NULL)
    {
        used = strlen(buffer);

        if (used >= size - 1)
        {
            pclose(fp);
            fprintf(stderr, "Command output too large\n");
            return -1;
        }
    }

    int status = pclose(fp);

    if (status != 0)
        return -1;

    return 0;
}

static int isSafeDeviceName(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return 0;

    for (int i = 0; name[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)name[i]))
            return 0;
    }

    return 1;
}

static int isValidDevicePath(const char *name, const char *path)
{
    char expected[128];

    snprintf(expected, sizeof(expected), "/dev/%s", name);

    return strcmp(expected, path) == 0;
}

int validateUsbDeviceForVentoy(UsbDevice *dev)
{
    if (dev == NULL ||
        !isSafeDeviceName(dev->name) ||
        !isValidDevicePath(dev->name, dev->dev_path))
    {
        fprintf(stderr, "Invalid USB device information\n");
        return -1;
    }

    struct stat st;

    if (stat(dev->dev_path, &st) != 0 || !S_ISBLK(st.st_mode))
    {
        fprintf(stderr, "%s is not a valid block device\n",
                dev->dev_path);
        return -1;
    }

    char command[512];
    char output[4096];

    snprintf(command, sizeof(command),
             "lsblk -dnro TRAN %s", dev->dev_path);

    if (readCommandOutput(command, output, sizeof(output)) != 0)
    {
        fprintf(stderr, "Cannot determine device transport\n");
        return -1;
    }

    output[strcspn(output, "\r\n")] = '\0';

    if (strcmp(output, "usb") != 0)
    {
        fprintf(stderr, "Device is not connected through USB: %s\n",
                dev->dev_path);
        return -1;
    }

    snprintf(command, sizeof(command),
             "lsblk -nrpo MOUNTPOINTS %s", dev->dev_path);

    if (readCommandOutput(command, output, sizeof(output)) != 0)
    {
        fprintf(stderr, "Cannot check mounted partitions\n");
        return -1;
    }

    for (char *p = output; *p != '\0'; p++)
    {
        if (!isspace((unsigned char)*p))
        {
            fprintf(stderr,
                    "Device or one of its partitions is mounted:\n%s",
                    output);
            return -1;
        }
    }

    if (readCommandOutput("findmnt -nro SOURCE /",
                          output, sizeof(output)) != 0)
    {
        fprintf(stderr, "Cannot determine root filesystem source\n");
        return -1;
    }

    output[strcspn(output, "\r\n")] = '\0';

    if (strncmp(output, "/dev/", 5) == 0)
    {
        for (char *p = output; *p != '\0'; p++)
        {
            if (!isalnum((unsigned char)*p) &&
                *p != '/' && *p != '-' && *p != '_')
            {
                fprintf(stderr, "Unexpected root device path\n");
                return -1;
            }
        }

        char *cmd[] = {
            "lsblk",
            "-s",
            "-nrpo",
            "NAME",
            output,
            NULL
        };

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

            execvp(cmd[0], cmd);
            _exit(127);
        }

        close(pipefd[1]);

        size_t total = 0;
        ssize_t n;

        while ((n = read(pipefd[0], output + total,
                         sizeof(output) - total - 1)) > 0)
        {
            total += (size_t)n;

            if (total >= sizeof(output) - 1)
            {
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                fprintf(stderr, "lsblk output too large\n");
                return -1;
            }
        }

        close(pipefd[0]);

        int status;

        if (waitpid(pid, &status, 0) == -1 ||
            !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            fprintf(stderr, "Cannot determine system disk\n");
            return -1;
        }

        output[total] = '\0';

        char *line = strtok(output, "\n");

        while (line != NULL)
        {
            if (strcmp(line, dev->dev_path) == 0)
            {
                fprintf(stderr,
                        "Refusing to modify the system disk: %s\n",
                        dev->dev_path);
                return -1;
            }

            line = strtok(NULL, "\n");
        }
    }

    printf("USB device validation passed: %s\n", dev->dev_path);
    return 0;
}
