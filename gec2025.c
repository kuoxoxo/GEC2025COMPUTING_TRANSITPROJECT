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
 *   out_stop  - Pointer to Stop struct to store matching stop data (can be
 * NULL)
 *
 * Returns:
 *   1 if stop found and displayed, 0 if no match or file error
 */
int find_stop_in_csv(const char* stopsPath, const char* query, Stop* out_stop) {
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
    char hdr[4096];
    strcpy(hdr, line);
    char* tok = strtok(hdr, ",");
    int idx = 0;
    while (tok) {
      // Remove newlines from token
      tok[strcspn(tok, "\r\n")] = 0;
      // Identify each column by header name
      if (strcmp(tok, "stop_id") == 0) idx_id = idx;
      if (strcmp(tok, "stop_name") == 0) idx_name = idx;
      if (strcmp(tok, "stop_lat") == 0) idx_lat = idx;
      if (strcmp(tok, "stop_lon") == 0) idx_lon = idx;
      tok = strtok(NULL, ",");
      idx++;
    }
  }

  // Debug: print found indices
  printf("DEBUG: idx_id=%d, idx_name=%d, idx_lat=%d, idx_lon=%d\n", idx_id,
         idx_name, idx_lat, idx_lon);

  // Convert query to lowercase for case-insensitive name matching
  char qlower[512];
  str_to_lower_copy(query, qlower, sizeof(qlower));

  int found = 0;
  // Search through each data row
  while (fgets(line, sizeof(line), fp)) {
    // Remove trailing newline
    line[strcspn(line, "\r\n")] = 0;

    // Parse CSV fields (naive split - does not handle quoted commas)
    char lineCopy[4096];
    strcpy(lineCopy, line);
    char* fields[128];
    int fc = 0;
    char* t = strtok(lineCopy, ",");
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

        // If out_stop provided, populate it with stop data
        if (out_stop) {
          out_stop->stop_id = malloc(strlen(fields[idx_id]) + 1);
          strcpy(out_stop->stop_id, fields[idx_id]);

          if (idx_name >= 0 && idx_name < fc) {
            out_stop->stop_name = malloc(strlen(fields[idx_name]) + 1);
            strcpy(out_stop->stop_name, fields[idx_name]);
          }

          if (idx_lat >= 0 && idx_lat < fc) {
            out_stop->stop_lat = atof(fields[idx_lat]);
            printf("DEBUG: lat field='%s' value=%.6f\n", fields[idx_lat],
                   out_stop->stop_lat);
          }
          if (idx_lon >= 0 && idx_lon < fc) {
            out_stop->stop_lon = atof(fields[idx_lon]);
            printf("DEBUG: lon field='%s' value=%.6f\n", fields[idx_lon],
                   out_stop->stop_lon);
          }
        }
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

        // If out_stop provided, populate it with stop data
        if (out_stop) {
          if (idx_id >= 0 && idx_id < fc) {
            out_stop->stop_id = malloc(strlen(fields[idx_id]) + 1);
            strcpy(out_stop->stop_id, fields[idx_id]);
          }

          out_stop->stop_name = malloc(strlen(fields[idx_name]) + 1);
          strcpy(out_stop->stop_name, fields[idx_name]);

          if (idx_lat >= 0 && idx_lat < fc)
            out_stop->stop_lat = atof(fields[idx_lat]);
          if (idx_lon >= 0 && idx_lon < fc)
            out_stop->stop_lon = atof(fields[idx_lon]);
        }
        break;
      }
    }
  }

  fclose(fp);
  if (!found) printf("No matching stop found for '%s'.\n", query);
  return found;
}

/**
 * get_intermediate_stops()
 *
 * Retrieves all stops on a route between the origin and final stops.
 * Reads from stop_times.csv and stops.csv to find all stops in sequence.
 *
 * Parameters:
 *   stop_times_path - Path to stop_times.csv
 *   stops_path      - Path to stops.csv
 *   origin_id       - Stop ID of the origin stop
 *   final_id        - Stop ID of the final stop
 *   stops_array     - Output array of Stop structs
 *   max_stops       - Maximum number of stops to return
 *
 * Returns:
 *   Number of stops in the route (including origin and final)
 */
int get_intermediate_stops(const char* stop_times_path, const char* stops_path,
                           const char* origin_id, const char* final_id,
                           Stop* stops_array, int max_stops) {
  FILE* fp = fopen(stop_times_path, "r");
  char resolved[1200];

  // If file not found, try to find it
  if (!fp) {
    if (find_file_in_ancestors(stop_times_path, resolved, sizeof(resolved),
                               6)) {
      fp = fopen(resolved, "r");
    }
    if (!fp) return 0;
  } else {
    strcpy(resolved, stop_times_path);
  }

  char line[4096];
  int trip_idx = -1, stop_idx = -1, seq_idx = -1;
  int found_trip = 0;
  char trip_id[256] = "";
  StopTime* stop_sequence = (StopTime*)malloc(max_stops * sizeof(StopTime));
  int seq_count = 0;

  // Read header
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    free(stop_sequence);
    return 0;
  }

  // Parse header
  char hdr[4096];
  strcpy(hdr, line);
  char* tok = strtok(hdr, ",");
  int idx = 0;
  while (tok) {
    tok[strcspn(tok, "\r\n")] = 0;
    if (strcmp(tok, "trip_id") == 0) trip_idx = idx;
    if (strcmp(tok, "stop_id") == 0) stop_idx = idx;
    if (strcmp(tok, "stop_sequence") == 0) seq_idx = idx;
    tok = strtok(NULL, ",");
    idx++;
  }

  // Find a trip that contains both origin and final stops
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = 0;

    char lineCopy[4096];
    strcpy(lineCopy, line);
    char* fields[128];
    int fc = 0;
    char* t = strtok(lineCopy, ",");
    while (t && fc < 128) {
      fields[fc++] = t;
      t = strtok(NULL, ",");
    }

    if (trip_idx >= 0 && trip_idx < fc && stop_idx >= 0 && stop_idx < fc) {
      // If this is a new trip, check if previous one contained both stops
      if (strcmp(fields[trip_idx], trip_id) != 0) {
        if (seq_count > 0) {
          // Check if both origin and final stops are in this trip
          int has_origin = 0, has_final = 0;
          for (int i = 0; i < seq_count; i++) {
            if (strcmp(stop_sequence[i].stop_id, origin_id) == 0)
              has_origin = 1;
            if (strcmp(stop_sequence[i].stop_id, final_id) == 0) has_final = 1;
          }

          if (has_origin && has_final) {
            found_trip = 1;
            break;
          }
        }
        // Reset for new trip
        strcpy(trip_id, fields[trip_idx]);
        seq_count = 0;
      }

      // Add this stop to the sequence
      if (seq_count < max_stops) {
        stop_sequence[seq_count].stop_id =
            (char*)malloc(strlen(fields[stop_idx]) + 1);
        strcpy(stop_sequence[seq_count].stop_id, fields[stop_idx]);
        if (seq_idx >= 0 && seq_idx < fc) {
          stop_sequence[seq_count].stop_sequence = atoi(fields[seq_idx]);
        }
        seq_count++;
      }
    }
  }

  fclose(fp);

  // If we found a valid trip, now get the coordinates for each stop
  if (found_trip && seq_count > 0) {
    // Sort by stop sequence
    for (int i = 0; i < seq_count - 1; i++) {
      for (int j = 0; j < seq_count - i - 1; j++) {
        if (stop_sequence[j].stop_sequence >
            stop_sequence[j + 1].stop_sequence) {
          StopTime temp = stop_sequence[j];
          stop_sequence[j] = stop_sequence[j + 1];
          stop_sequence[j + 1] = temp;
        }
      }
    }

    // Find the range from origin to final
    int start_idx = -1, end_idx = -1;
    for (int i = 0; i < seq_count; i++) {
      if (strcmp(stop_sequence[i].stop_id, origin_id) == 0) start_idx = i;
      if (strcmp(stop_sequence[i].stop_id, final_id) == 0) {
        end_idx = i;
        break;
      }
    }

    // Get stop details for all stops in the range
    int stops_count = 0;
    if (start_idx >= 0 && end_idx >= start_idx) {
      for (int i = start_idx; i <= end_idx && stops_count < max_stops; i++) {
        if (find_stop_in_csv(stops_path, stop_sequence[i].stop_id,
                             &stops_array[stops_count])) {
          stops_count++;
        }
      }
    }

    // Free the sequence array
    for (int i = 0; i < seq_count; i++) {
      if (stop_sequence[i].stop_id) free(stop_sequence[i].stop_id);
    }
    free(stop_sequence);

    return stops_count;
  }

  // Cleanup
  for (int i = 0; i < seq_count; i++) {
    if (stop_sequence[i].stop_id) free(stop_sequence[i].stop_id);
  }
  free(stop_sequence);

  return 0;
}

/**
 * generate_map_html()
 *
 * Generates an HTML map file with Leaflet.js that displays stops
 * and a line connecting them through all intermediate stops.
 *
 * Parameters:
 *   origin_stop  - Pointer to the origin Stop struct
 *   final_stop   - Pointer to the final Stop struct
 *   output_path  - Path where to write the HTML file
 *   stops_path   - Path to stops.csv for fetching intermediate stops
 *   stop_times_path - Path to stop_times.csv
 */
void generate_map_html(Stop* origin_stop, Stop* final_stop,
                       const char* output_path, const char* stops_path,
                       const char* stop_times_path) {
  FILE* fp = fopen(output_path, "w");
  if (!fp) {
    fprintf(stderr, "Failed to create map file: %s\n", output_path);
    return;
  }

  // Fetch intermediate stops
  Stop* intermediate_stops = (Stop*)malloc(100 * sizeof(Stop));
  memset(intermediate_stops, 0, 100 * sizeof(Stop));

  int stops_count =
      get_intermediate_stops(stop_times_path, stops_path, origin_stop->stop_id,
                             final_stop->stop_id, intermediate_stops, 100);

  // If we couldn't find intermediate stops, use just origin and final
  if (stops_count == 0) {
    printf("Could not find intermediate stops, using direct connection\n");
    intermediate_stops[0] = *origin_stop;
    intermediate_stops[1] = *final_stop;
    stops_count = 2;
  }

  // Calculate center point
  double center_lat = 0.0, center_lon = 0.0;
  for (int i = 0; i < stops_count; i++) {
    center_lat += intermediate_stops[i].stop_lat;
    center_lon += intermediate_stops[i].stop_lon;
  }
  center_lat /= stops_count;
  center_lon /= stops_count;

  // Write HTML header and map setup
  fprintf(fp,
          "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<meta "
          "name=\"viewport\" content=\"width=device-width, "
          "initial-scale=1.0\">\n");
  fprintf(fp,
          "<link rel=\"stylesheet\" "
          "href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\" />\n");
  fprintf(fp, "<style>\n");
  fprintf(fp, "  html, body, #map { height: 100%%; margin: 0; padding: 0; }\n");
  fprintf(fp,
          "  .info { padding: 10px; background: white; border-radius: "
          "5px; box-shadow: 0 0 15px rgba(0,0,0,0.2); }\n");
  fprintf(fp, "</style>\n</head>\n<body>\n");
  fprintf(fp, "<div id=\"map\"></div>\n");

  // Write JavaScript to create map and display stops
  fprintf(fp,
          "<script "
          "src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\"></"
          "script>\n");
  fprintf(fp, "<script>\n");
  fprintf(fp, "  var map = L.map('map').setView([%.6f, %.6f], 13);\n",
          center_lat, center_lon);
  fprintf(fp,
          "  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', "
          "{ maxZoom: 19 }).addTo(map);\n");

  // Draw polyline through all stops
  fprintf(fp, "  var latlngs = [\n");
  for (int i = 0; i < stops_count; i++) {
    fprintf(fp, "    [%.6f, %.6f]%s\n", intermediate_stops[i].stop_lat,
            intermediate_stops[i].stop_lon, (i < stops_count - 1) ? "," : "");
  }
  fprintf(fp, "  ];\n");
  fprintf(fp,
          "  var polyline = L.polyline(latlngs, {color: 'blue', weight: 3, "
          "opacity: 0.7}).addTo(map);\n");

  // Add markers for all stops
  for (int i = 0; i < stops_count; i++) {
    const char* marker_color = "blue";
    if (i == 0)
      marker_color = "green";  // Origin
    else if (i == stops_count - 1)
      marker_color = "red";  // Final
    else
      marker_color = "blue";  // Intermediate

    fprintf(fp, "  var marker%d = L.marker([%.6f, %.6f]).addTo(map)\n", i,
            intermediate_stops[i].stop_lat, intermediate_stops[i].stop_lon);
    fprintf(fp,
            "    .bindPopup('<b>%s: %s</b><br/>ID: %s<br/>Lat: %.6f, Lon: "
            "%.6f');\n",
            (i == 0)                 ? "Origin"
            : (i == stops_count - 1) ? "Final"
                                     : "Stop",
            intermediate_stops[i].stop_name ? intermediate_stops[i].stop_name
                                            : "Unknown",
            intermediate_stops[i].stop_id ? intermediate_stops[i].stop_id : "",
            intermediate_stops[i].stop_lat, intermediate_stops[i].stop_lon);
    fprintf(fp,
            "  marker%d.setIcon(L.icon({iconUrl: "
            "'https://raw.githubusercontent.com/pointhi/leaflet-color-markers/"
            "master/img/marker-%s.png', iconSize: [25, 41], iconAnchor: [12, "
            "41], popupAnchor: [1, -34]}));\n",
            i, marker_color);
  }

  fprintf(fp, "  map.fitBounds(polyline.getBounds());\n");
  fprintf(fp, "</script>\n");
  fprintf(fp, "</body>\n</html>\n");

  fclose(fp);

  // Free intermediate stops
  for (int i = 0; i < stops_count; i++) {
    if (intermediate_stops[i].stop_id) free(intermediate_stops[i].stop_id);
    if (intermediate_stops[i].stop_name) free(intermediate_stops[i].stop_name);
  }
  free(intermediate_stops);

  printf("Map generated: %s\n", output_path);
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
 * 5. Generate an interactive HTML map showing both stops
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

  // Structures to store stop data
  Stop origin_stop = {NULL, NULL, NULL, 0.0, 0.0};
  Stop final_stop = {NULL, NULL, NULL, 0.0, 0.0};

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

  // Search for and display origin stop, storing data in origin_stop struct
  printf("\nOrigin Stop:\n");
  if (!find_stop_in_csv(stopsPath, origin_input, &origin_stop)) {
    printf("Could not find origin stop. Exiting.\n");
    return 1;
  }

  // Search for and display final stop, storing data in final_stop struct
  printf("\nFinal Stop:\n");
  if (!find_stop_in_csv(stopsPath, final_input, &final_stop)) {
    printf("Could not find final stop. Exiting.\n");
    return 1;
  }

  // Generate map HTML file with both stops
  char mapPath[1200];
  char csvDir[1024];

  // Find the csv_files directory
  if (find_file_in_ancestors("./csv_files/stops.csv", csvDir, sizeof(csvDir),
                             6)) {
    // Extract just the directory path (remove the filename)
    char* lastSlash = strrchr(csvDir, '\\');
    if (lastSlash) {
      *lastSlash = '\0';
    }
    snprintf(mapPath, sizeof(mapPath), "%s\\route_map.html", csvDir);
  } else {
    // Fallback to current directory
    snprintf(mapPath, sizeof(mapPath), "route_map.html");
  }

  printf("\nGenerating map...\n");
  generate_map_html(&origin_stop, &final_stop, mapPath, stopsPath,
                    "./csv_files/stop_times.csv");
  printf("Map generated successfully!\n");
  printf("Open the following file in your browser:\n%s\n", mapPath);
  printf("\nOr use: Start-Process \"%s\"\n", mapPath);  // Free allocated memory
  if (origin_stop.stop_id) free(origin_stop.stop_id);
  if (origin_stop.stop_name) free(origin_stop.stop_name);
  if (final_stop.stop_id) free(final_stop.stop_id);
  if (final_stop.stop_name) free(final_stop.stop_name);

  return 0;
}
