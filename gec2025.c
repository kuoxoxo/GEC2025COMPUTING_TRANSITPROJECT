#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  if (!fp) {
    perror("opening stops file");
    return 0;
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
      "C:\\Users\\theme\\Desktop\\UNI\\GEC_2025\\csv_files\\routes.csv",
      "C:\\Users\\theme\\Desktop\\UNI\\GEC_2025\\csv_files\\shapes.csv",
      "C:\\Users\\theme\\Desktop\\UNI\\GEC_2025\\csv_files\\stops.csv",
      "C:\\Users\\theme\\Desktop\\UNI\\GEC_2025\\csv_files\\stop_times.csv",
      "C:\\Users\\theme\\Desktop\\UNI\\GEC_2025\\csv_files\\trips.csv"};

  char input[256];
  printf("Enter stop name or stop_id: ");
  if (!read_line(input, sizeof(input))) return 0;
  if (strlen(input) == 0) {
    printf("No input provided. Exiting.\n");
    return 0;
  }

  const char* stopsPath =
      "C:\\Users\\theme\\Desktop\\UNI\\GEC_2025\\csv_files\\stops.csv";
  find_stop_in_csv(stopsPath, input);

  return 0;
}
