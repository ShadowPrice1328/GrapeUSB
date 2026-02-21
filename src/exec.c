#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "exec.h"

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