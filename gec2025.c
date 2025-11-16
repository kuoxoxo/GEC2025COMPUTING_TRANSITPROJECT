#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

/**
 * GTFS Stop Lookup Program
 *
 * This program reads GTFS (General Transit Feed Specification) CSV files
 * and allows users to search for transit stops by name or ID.
 * It prompts for origin and final stop inputs and displays matching stop
 * information.
 */

/**
 * find_file_in_ancestors()
 *
 * Searches for a file by traversing up the directory tree from the current
 * working directory. Attempts to find the file at progressively higher
 * directory levels (up to max_levels).
 *
 * Parameters:
 *   relPath    - Relative path to the file to search for (e.g.,
 * "./csv_files/stops.csv") outPath    - Output buffer to store the resolved
 * absolute path if found outSize    - Size of the output buffer max_levels -
 * Maximum number of parent directories to traverse
 *
 * Returns:
 *   1 if file found (path stored in outPath), 0 otherwise
 */
int find_file_in_ancestors(const char* relPath, char* outPath, size_t outSize,
                           int max_levels) {
  char cwd[1024];
  // Get the current working directory
  if (_getcwd(cwd, sizeof(cwd)) == NULL) return 0;

  // Skip leading ./ or .\ in the relative path for cleaner concatenation
  const char* rel = relPath;
  if ((rel[0] == '.' && (rel[1] == '/' || rel[1] == '\\'))) rel += 2;

  // Try each directory level
  for (int level = 0; level <= max_levels; ++level) {
    char base[1024];
    // Copy current working directory to base
    strncpy(base, cwd, sizeof(base));
    base[sizeof(base) - 1] = '\0';

    // Move up 'level' parent directories
    for (int i = 0; i < level; ++i) {
      // Find the last backslash or forward slash in the path
      char* p = strrchr(base, '\\');
      if (!p) p = strrchr(base, '/');
      if (!p) {
        base[0] = '\0';
        break;
      }
      // Truncate path at the last separator
      *p = '\0';
    }

    // Build the candidate file path
    char candidate[1200];
    if (base[0] == '\0')
      snprintf(candidate, sizeof(candidate), "%s", rel);
    else
      snprintf(candidate, sizeof(candidate), "%s\\%s", base, rel);

    // Check if the candidate file exists
    FILE* f = fopen(candidate, "r");
    if (f) {
      fclose(f);
      // File found - store the path and return success
      strncpy(outPath, candidate, outSize);
      outPath[outSize - 1] = '\0';
      return 1;
    }
  }
  // File not found at any level
  return 0;
}

/**
 * get_exe_dir()
 *
 * Gets the directory containing the currently running executable.
 * Useful for finding data files relative to the executable location,
 * especially when the program is run from a debugger or different working
 * directory.
 *
 * Parameters:
 *   out      - Output buffer to store the executable directory path
 *   outSize  - Size of the output buffer
 *
 * Returns:
 *   1 on success, 0 on failure
 */
int get_exe_dir(char* out, size_t outSize) {
  char path[MAX_PATH];
  // Get the full path to the currently running executable
  DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
  if (len == 0 || len == MAX_PATH) return 0;

  // Find the last backslash or forward slash to separate directory from
  // filename
  char* p = strrchr(path, '\\');
  if (!p) p = strrchr(path, '/');
  if (!p) return 0;

  // Truncate the path at the last separator to get just the directory
  *p = '\0';
  strncpy(out, path, outSize);
  out[outSize - 1] = '\0';
  return 1;
}

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Stop structure
 * Represents a transit stop from the stops.csv file.
 */
typedef struct {
  char* stop_id;    ///< Unique identifier for the stop
  char* stop_name;  ///< Name of the stop
  char* stop_desc;  ///< Description of the stop
  double stop_lat;  ///< Latitude coordinate
  double stop_lon;  ///< Longitude coordinate
} Stop;

/**
 * StopTime structure
 * Represents a stop time from the stop_times.csv file.
 * Tracks when a trip stops at a particular stop.
 */
typedef struct {
  char* trip_id;         ///< ID of the trip
  char* arrival_time;    ///< Arrival time at this stop
  char* departure_time;  ///< Departure time from this stop
  char* stop_id;         ///< ID of the stop
  int stop_sequence;     ///< Sequence number of stop in the trip
} StopTime;

/**
 * Trip structure
 * Represents a trip from the trips.csv file.
 * A trip is a sequence of stops that a vehicle travels along on a specific
 * route.
 */
typedef struct {
  char* route_id;       ///< ID of the route this trip belongs to
  char* service_id;     ///< Service ID for schedule patterns
  char* trip_id;        ///< Unique identifier for the trip
  char* trip_headsign;  ///< Direction/destination displayed on the vehicle
  int direction_id;     ///< Direction ID (0 or 1, typically)
} Trip;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * readCSVFile()
 *
 * Reads and prints the first 10 lines of a CSV file for inspection.
 * Helper function for debugging CSV files.
 *
 * Parameters:
 *   filename - Path to the CSV file to read
 */
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

  // Read and parse lines from the file
  while (fgets(line, sizeof(line), fp) != NULL && lineCount < maxLines) {
    // Remove newline character
    line[strcspn(line, "\n")] = 0;

    char* field;
    char lineCopy[1024];
    strcpy(lineCopy, line);

    // Parse comma-separated values
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

/**
 * read_line()
 *
 * Safely reads a line from standard input and removes trailing newline/carriage
 * return.
 *
 * Parameters:
 *   buf  - Buffer to store the input line
 *   size - Size of the buffer
 *
 * Returns:
 *   1 on success, 0 on failure (EOF or read error)
 */
int read_line(char* buf, size_t size) {
  if (!fgets(buf, (int)size, stdin)) return 0;
  // Remove trailing \r and \n characters
  buf[strcspn(buf, "\r\n")] = '\0';
  return 1;
}

/**
 * str_to_lower_copy()
 *
 * Creates a lowercase copy of a string for case-insensitive comparisons.
 *
 * Parameters:
 *   src       - Source string to convert
 *   dest      - Destination buffer for lowercase copy
 *   destSize  - Size of destination buffer
 */
void str_to_lower_copy(const char* src, char* dest, size_t destSize) {
  size_t i;
  // Convert each character to lowercase
  for (i = 0; i + 1 < destSize && src[i] != '\0'; ++i)
    dest[i] = (char)tolower((unsigned char)src[i]);
  // Null-terminate the destination string
  dest[i] = '\0';
}

// ============================================================================
// STOP SEARCH FUNCTIONS
// ============================================================================

/**
 * find_stop_in_csv()
 *
 * Searches for a stop in the stops.csv file by stop_id or stop_name.
 * Performs exact matching on stop_id and substring matching on stop_name.
 * Case-insensitive for name matching.
 *
 * The function attempts to locate the CSV file in multiple locations:
 * 1. The path provided (stops.csv in current directory)
 * 2. Parent directories (walks up the directory tree)
 * 3. The executable's directory
 *
 * Parameters:
 *   stopsPath - Path to the stops.csv file
 *   query     - The stop_id or name to search for
 *
 * Returns:
 *   1 if stop found and displayed, 0 if no match or file error
 */
int find_stop_in_csv(const char* stopsPath, const char* query) {
  FILE* fp = fopen(stopsPath, "r");
  char resolved[1200];

  // If file not found at the provided path, search for it
  if (!fp) {
    // Try from current working directory and parent directories
    if (find_file_in_ancestors(stopsPath, resolved, sizeof(resolved), 6)) {
      printf("found stops file at: %s\n", resolved);
      fp = fopen(resolved, "r");
      if (!fp) {
        fprintf(stderr, "opening stops file '%s': %s\n", resolved,
                strerror(errno));
        return 0;
      }
    } else {
      // Try from executable directory (useful when debugger runs with different
      // cwd)
      char exeDir[1024];
      if (get_exe_dir(exeDir, sizeof(exeDir))) {
        char oldcwd[1024];
        if (_getcwd(oldcwd, sizeof(oldcwd)) != NULL) {
          if (_chdir(exeDir) == 0) {
            if (find_file_in_ancestors(stopsPath, resolved, sizeof(resolved),
                                       6)) {
              printf("found stops file at: %s\n", resolved);
              fp = fopen(resolved, "r");
            }
            _chdir(oldcwd);
          }
        }
      }
    }

    if (!fp) {
      fprintf(stderr, "opening stops file '%s': %s\n", stopsPath,
              strerror(errno));
      return 0;
    }
  }

  // Read the first line (header) to determine column indices
  char line[4096];
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return 0;
  }

  // Parse header to find column positions for stop_id, stop_name, stop_lat,
  // stop_lon
  int idx_id = -1, idx_name = -1, idx_lat = -1, idx_lon = -1;
  {
    char* hdr = strdup(line);
    char* tok = strtok(hdr, ",");
    int idx = 0;
    while (tok) {
      // Identify each column by header name
      if (strcmp(tok, "stop_id") == 0) idx_id = idx;
      if (strcmp(tok, "stop_name") == 0) idx_name = idx;
      if (strcmp(tok, "stop_lat") == 0) idx_lat = idx;
      if (strcmp(tok, "stop_lon") == 0) idx_lon = idx;
      tok = strtok(NULL, ",");
      idx++;
    }
    free(hdr);
  }

  // Convert query to lowercase for case-insensitive name matching
  char qlower[512];
  str_to_lower_copy(query, qlower, sizeof(qlower));

  int found = 0;
  // Search through each data row
  while (fgets(line, sizeof(line), fp)) {
    // Remove trailing newline
    line[strcspn(line, "\r\n")] = 0;

    // Parse CSV fields (naive split - does not handle quoted commas)
    char* fields[128];
    int fc = 0;
    char* p = line;
    char* t = strtok(p, ",");
    while (t && fc < 128) {
      fields[fc++] = t;
      t = strtok(NULL, ",");
    }

    // Check for exact match on stop_id
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

    // Check for substring match on stop_name (case-insensitive)
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

// ============================================================================
// MAIN PROGRAM
// ============================================================================

/**
 * main()
 *
 * Entry point for the GTFS stop lookup program.
 *
 * Process:
 * 1. Prompt user for origin stop (by name or ID)
 * 2. Prompt user for final/destination stop (by name or ID)
 * 3. Search and display the origin stop details
 * 4. Search and display the final stop details
 *
 * Returns:
 *   0 on successful completion, error code on failure
 */
int main() {
  // Array of CSV files used by this program (for reference)
  const char* csvFiles[] = {"./csv_files/routes.csv", "./csv_files/shapes.csv",
                            "./csv_files/stops.csv",
                            "./csv_files/stop_times.csv",
                            "./csv_files/trips.csv"};

  // Buffers to store user input for origin and final stops
  char origin_input[256];
  char final_input[256];

  // Prompt for and read origin stop input
  printf("Enter origin stop name or stop_id: ");
  if (!read_line(origin_input, sizeof(origin_input))) return 0;
  if (strlen(origin_input) == 0) {
    printf("No origin provided. Exiting.\n");
    return 0;
  }

  // Prompt for and read final stop input
  printf("Enter final stop name or stop_id: ");
  if (!read_line(final_input, sizeof(final_input))) return 0;
  if (strlen(final_input) == 0) {
    printf("No final stop provided. Exiting.\n");
    return 0;
  }

  // Path to the stops CSV file
  const char* stopsPath = "./csv_files/stops.csv";

  // Search for and display origin stop
  printf("\nOrigin Stop:\n");
  find_stop_in_csv(stopsPath, origin_input);

  // Search for and display final stop
  printf("\nFinal Stop:\n");
  find_stop_in_csv(stopsPath, final_input);

  return 0;
}
