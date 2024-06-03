#include "../include/process.h"

int main(void) {
    Process proc;
#ifdef _WIN32
    const char* command = "ping";
    const char* const argv[] = {"ping", "google.com", NULL};
#else
    const char* command = "ls";

    // helper macro to create NULL terminated arrays
    const char* const* argv = ENV_ARRAY("ls", "-l", "-a");
#endif
    const char* const* envp = DEFAULT_ENVP;
    // or {NULL} for envp if you want to inherit the environment of the calling process
    // or {"VAR1=VALUE1", "VAR2=VALUE2", NULL} to set the environment variables

    if (process_create(&proc, command, argv, envp) != 0) {
        return 1;
    }

    int status;
    return process_wait(&proc, &status);
}