#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "jsmn.h"
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