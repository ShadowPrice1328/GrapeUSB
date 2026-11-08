
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "exec.h"

int run(char *const argv[])
{
    if (argv == NULL || argv[0] == NULL)
    {
        fprintf(stderr, "Invalid command arguments\n");
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        return -1;
    }

    if (pid == 0)
    {
        execvp(argv[0], argv);

        perror("execvp failed");
        _exit(127);
    }

    int status;
    pid_t result;

    do
    {
        result = waitpid(pid, &status, 0);
    }
    while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        perror("waitpid failed");
        return -1;
    }

    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);

        if (exit_code != 0)
        {
            fprintf(stderr,
                    "Command failed with exit code %d: %s\n",
                    exit_code, argv[0]);
        }

        return exit_code;
    }

    if (WIFSIGNALED(status))
    {
        fprintf(stderr,
                "Command killed by signal %d: %s\n",
                WTERMSIG(status), argv[0]);
    }

    return -1;
}

int run_checked(char *const argv[])
{
    int result = run(argv);

    if (result != 0)
    {
        const char *name = (argv != NULL && argv[0] != NULL)
            ? argv[0]
            : "(invalid command)";

        fprintf(stderr, "Failed to execute: %s\n", name);
    }

    return result;
}