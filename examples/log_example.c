#define LOG_IMPLEMENTATION
#include "../log.h"

int main() {
  FILE *fp = fopen("log.txt", "w");
  if (fp == NULL) {
    return -1;
  }

  log_setfile(fp);
  log_setflags(LOG_FLAG_ALL);
  log_settimeformat("%A, %F %r");

  DEBUG("Hello my friends\n");
  fclose(fp);

  return 0;
}
