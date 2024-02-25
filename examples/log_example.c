#define LOG_IMPL
#include "../log.h"

int main() {
    FILE* fp = fopen("log.txt", "w");
    if (fp == NULL) {
        return -1;
    }

    log_setfile(fp);
    log_setflags(LOG_FLAG_ALL);
    log_settimeformat("%A, %F %r");
    log_setlevel(LOG_INFO);

    DEBUG("Hello my friends::DEBUG\n");  // This will not be printed
    INFO("Hello my friends::INFo\n");    // This will not be printed
    WARN("Hello my friends:: WARN\n");   // This will not be printed
    ERROR("Hello my friends::ERROR\n");  // This will be printed
    fclose(fp);

    return 0;
}
