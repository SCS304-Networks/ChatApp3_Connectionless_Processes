#ifndef UTILS_H
#define UTILS_H

// Generates the current timestamp in HH:MM format
void generate_timestamp(char buffer[]);

// Strips unwanted characters from input (newlines, pipes)
void sanitize_input(char buffer[]);

// Returns the size of a file in bytes
long fetch_byte_count(char filename[]);

// Parses a pipe-delimited request string into fields
void parse_request(char buffer[], char fields[][256], int *count);

// Creates a file if it does not exist
int create_file_if_missing(char filename[]);

// Initializes the file lock for process-safe data access
void init_data_mutex();

// Acquires the file lock (blocks until available)
void lock_data_files();

// Releases the file lock
void unlock_data_files();

// Destroys the file lock (call on shutdown)
void destroy_data_mutex();

#endif // UTILS_H
