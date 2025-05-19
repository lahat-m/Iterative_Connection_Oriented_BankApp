/*
 * Banking System Client - Logging Module
 */

#include "bank_client.h"

/* Global variable */
FILE *log_file = NULL;

/* Initialize the logging system */
void log_init(void) {
    // Only initialize if not already initialized
    if (log_file) {
        return;
    }
    
    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Failed to open log file");
        log_file = stderr; /* Fallback to stderr if file cannot be opened */
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log_file, "\n[%s] [INFO] ========== BANK CLIENT STARTED ==========\n", timestamp);
    fflush(log_file);
}

/* Close the logging system */
void log_close(void) {
    if (log_file && log_file != stderr) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        fprintf(log_file, "[%s] [INFO] ========== BANK CLIENT STOPPED ==========\n", timestamp);
        fflush(log_file);
        fclose(log_file);
        log_file = NULL; // Prevent double closing
    }
}

/* Log a message with the specified level */
void log_message(log_level_t level, const char *format, ...) {
    if (!log_file) {
        log_init();
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    const char *level_str;
    switch (level) {
        case LOG_INFO:    level_str = "INFO"; break;
        case LOG_WARNING: level_str = "WARNING"; break;
        case LOG_ERROR:   level_str = "ERROR"; break;
        default:          level_str = "UNKNOWN"; break;
    }
    
    fprintf(log_file, "[%s] [%s] ", timestamp, level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}