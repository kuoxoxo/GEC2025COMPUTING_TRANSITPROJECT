#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <direct.h>
// additional helpers
#include <errno.h>
#include <sys/stat.h>
#include <windows.h>

// Try to find a relative path by looking in the current directory and then
// walking up parent directories up to max_levels. If found, writes the
// absolute candidate path into outPath and returns 1, otherwise 0.
int find_file_in_ancestors(const char* relPath, char* outPath, size_t outSize,
                           int max_levels) {
  char cwd[1024];
  if (_getcwd(cwd, sizeof(cwd)) == NULL) return 0;

  // Skip a leading ./ or .\ in relPath for clean concatenation
  const char* rel = relPath;
  if ((rel[0] == '.' && (rel[1] == '/' || rel[1] == '\\')))
    rel += 2;

  for (int level = 0; level <= max_levels; ++level) {
    char base[1024];
    strncpy(base, cwd, sizeof(base));
    base[sizeof(base) - 1] = '\0';

    // Move up `level` parents
    for (int i = 0; i < level; ++i) {
      char* p = strrchr(base, '\\');
      if (!p) p = strrchr(base, '/');
      if (!p) {
        base[0] = '\0';
        break;
      }
      *p = '\0';
    }

    // Build candidate path
    char candidate[1200];
    if (base[0] == '\0')
      snprintf(candidate, sizeof(candidate), "%s", rel);
    else
      snprintf(candidate, sizeof(candidate), "%s\\%s", base, rel);

    FILE* f = fopen(candidate, "r");
    if (f) {
      fclose(f);
      strncpy(outPath, candidate, outSize);
      outPath[outSize - 1] = '\0';
      return 1;
    }
  }
  return 0;
}

// Helper: get directory containing the running executable. Returns 1 on
// success, 0 on failure.
int get_exe_dir(char* out, size_t outSize) {
  char path[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
  if (len == 0 || len == MAX_PATH) return 0;
  char* p = strrchr(path, '\\');
  if (!p) p = strrchr(path, '/');
  if (!p) return 0;
  *p = '\0';
  strncpy(out, path, outSize);
  out[outSize - 1] = '\0';
  return 1;
}
// structs
typedef struct {
  char* stop_id;
  char* stop_name;
  char* stop_desc;
  double stop_lat;
  double stop_lon;
} Stop;

typedef struct {
  char* trip_id;
  char* arrival_time;
  char* departure_time;
  char* stop_id;
  int stop_sequence;
} StopTime;

typedef struct {
  char* route_id;
  char* service_id;
  char* trip_id;
  char* trip_headsign;
  int direction_id;
} Trip;
// function written by copiliot
void readCSVFile(const char* filename) {
  FILE* fp = fopen(filename, "r");
  if (fp == NULL) {
    perror("File opening error");
    return;
  }

  printf("\n=== Reading %s ===\n", filename);
  char line[1024];
  int lineCount = 0;
  int maxLines = 10;  // Limit to first 10 lines per file

  while (fgets(line, sizeof(line), fp) != NULL && lineCount < maxLines) {
    line[strcspn(line, "\n")] = 0;

    char* field;
    char lineCopy[1024];
    strcpy(lineCopy, line);

    field = strtok(lineCopy, ",");
    while (field != NULL) {
      printf("%s | ", field);
      field = strtok(NULL, ",");
    }
    printf("\n");
    lineCount++;
  }

  if (lineCount == maxLines) {
    printf("... (limiting output to %d lines)\n", maxLines);
  }

  fclose(fp);
}

// Read a line from stdin and strip newline. Returns 1 on success.
int read_line(char* buf, size_t size) {
  if (!fgets(buf, (int)size, stdin)) return 0;
  buf[strcspn(buf, "\r\n")] = '\0';
  return 1;
}

// Lowercase copy
void str_to_lower_copy(const char* src, char* dest, size_t destSize) {
  size_t i;
  for (i = 0; i + 1 < destSize && src[i] != '\0'; ++i)
    dest[i] = (char)tolower((unsigned char)src[i]);
  dest[i] = '\0';
}

// Search stops.csv for an exact stop_id or a substring match on stop_name.
// If found, prints the stop_name (and id) and returns 1. Otherwise prints a
// message and returns 0.
int find_stop_in_csv(const char* stopsPath, const char* query) {
  FILE* fp = fopen(stopsPath, "r");
  char resolved[1200];
  if (!fp) {
    // Try from current working directory and parents
    if (find_file_in_ancestors(stopsPath, resolved, sizeof(resolved), 6)) {
      printf("found stops file at: %s\n", resolved);
      fp = fopen(resolved, "r");
      if (!fp) {
        fprintf(stderr, "opening stops file '%s': %s\n", resolved, strerror(errno));
        return 0;
      }
    } else {
      // Try from executable directory (useful when debugger runs with different cwd)
      char exeDir[1024];
      if (get_exe_dir(exeDir, sizeof(exeDir))) {
        char oldcwd[1024];
        if (_getcwd(oldcwd, sizeof(oldcwd)) != NULL) {
          if (_chdir(exeDir) == 0) {
            if (find_file_in_ancestors(stopsPath, resolved, sizeof(resolved), 6)) {
              printf("found stops file at: %s\n", resolved);
              fp = fopen(resolved, "r");
            }
            _chdir(oldcwd);
          }
        }
      }
    }

    if (!fp) {
      fprintf(stderr, "opening stops file '%s': %s\n", stopsPath, strerror(errno));
      return 0;
    }
  }

  char line[4096];
  // read header
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return 0;
  }

  // determine column indices for stop_id, stop_name, stop_lat, stop_lon
  int idx_id = -1, idx_name = -1, idx_lat = -1, idx_lon = -1;
  {
    char* hdr = strdup(line);
    char* tok = strtok(hdr, ",");
    int idx = 0;
    while (tok) {
      if (strcmp(tok, "stop_id") == 0) idx_id = idx;
      if (strcmp(tok, "stop_name") == 0) idx_name = idx;
      if (strcmp(tok, "stop_lat") == 0) idx_lat = idx;
      if (strcmp(tok, "stop_lon") == 0) idx_lon = idx;
      tok = strtok(NULL, ",");
      idx++;
    }
    free(hdr);
  }

  char qlower[512];
  str_to_lower_copy(query, qlower, sizeof(qlower));

  int found = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = 0;
    // naive split (does not handle quoted commas). Works for typical GTFS.
    char* fields[128];
    int fc = 0;
    char* p = line;
    char* t = strtok(p, ",");
    while (t && fc < 128) {
      fields[fc++] = t;
      t = strtok(NULL, ",");
    }

    if (idx_id >= 0 && idx_id < fc) {
      if (strcmp(fields[idx_id], query) == 0) {
        printf("Found stop: id=%s", fields[idx_id]);
        if (idx_name >= 0 && idx_name < fc)
          printf(" name=%s", fields[idx_name]);
        printf("\n");
        found = 1;
        break;
      }
    }

    if (idx_name >= 0 && idx_name < fc) {
      char nameLower[512];
      str_to_lower_copy(fields[idx_name], nameLower, sizeof(nameLower));
      if (strstr(nameLower, qlower) != NULL) {
        printf("Found stop: id=%s name=%s\n",
               (idx_id >= 0 && idx_id < fc) ? fields[idx_id] : "",
               fields[idx_name]);
        found = 1;
        break;
      }
    }
  }

  fclose(fp);
  if (!found) printf("No matching stop found for '%s'.\n", query);
  return found;
}

int main() {
  const char* csvFiles[] = {
      "./csv_files/routes.csv",
      "./csv_files/shapes.csv",
      "./csv_files/stops.csv",
      "./csv_files/stop_times.csv",
      "./csv_files/trips.csv"};

  char input[256];
  printf("Enter stop name or stop_id: ");
  if (!read_line(input, sizeof(input))) return 0;
  if (strlen(input) == 0) {
    printf("No input provided. Exiting.\n");
    return 0;
  }

  const char* stopsPath =
      "./csv_files/stops.csv";

  find_stop_in_csv(stopsPath, input);

  return 0;
}
