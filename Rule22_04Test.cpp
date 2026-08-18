#include <stdio.h>

static FILE *openLog(const char *Path, bool Resume) {
  const char *Mode = Resume ? "r" : "w";
  return fopen(Path, Mode);
}

static void closeLog(FILE *LogFile) { fclose(LogFile); }

static void runImport(const char *Path, bool Resume) {
  FILE *LogFile = openLog(Path, Resume);
  if (!LogFile)
    return;

  fprintf(LogFile, "event=import\n");
  closeLog(LogFile);
}

int main() {
  runImport("audit.log", /*Resume=*/true);
  return 0;
}
