#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Struct for storing image data
struct fs_data {
    int bytes_per_sector, sectors_per_cluster, reserved_sectors, number_of_fats, sectors_per_fat, num_logical_sectors;
    int max_root_directory_entries, max_entries;
    int media_descriptor;
    int root_directory_start;
    int fat_start, fat_size;
} data;

// Struct for storing entry data
struct entry {
    int create_date, day, month, year;
    int create_time, seconds, minutes, hours, ms;
    int access_date, access_day, access_month, access_year;
    int modify_date, modify_day, modify_month, modify_year;
    int modify_time, modify_seconds, modify_minutes, modify_hours, modify_ms;
} entry_data;

// Read from a specific place in the filesystem
unsigned int get_bytes(void *file_system, int offset, int size) {
    unsigned int bytes = 0;
    for (int i = 0; i < size; i++) {
        // Bitwise or the file system bytes into place and leftshift to read more up to size
        bytes |= ((unsigned char *)file_system)[offset + i] << i * 8;
    }
    return bytes;
}

// Add image data to structure
void build_fs_data(void *file_system) {
    data.bytes_per_sector = get_bytes(file_system, 0x00B, 2);
    data.sectors_per_cluster = get_bytes(file_system, 0x00D, 1);
    data.reserved_sectors = get_bytes(file_system, 0x00E, 2);
    data.number_of_fats = get_bytes(file_system, 0x010, 1);
    data.fat_start = data.bytes_per_sector * data.reserved_sectors;
    data.max_root_directory_entries = get_bytes(file_system, 0x011, 2);
    data.num_logical_sectors = get_bytes(file_system, 0x013, 2);
    data.media_descriptor = get_bytes(file_system, 0x015, 1);
    data.sectors_per_fat = get_bytes(file_system, 0x016, 2);
    data.fat_size = data.bytes_per_sector * data.sectors_per_fat;
    data.root_directory_start = (data.bytes_per_sector * data.reserved_sectors) + (data.bytes_per_sector * data.sectors_per_fat * data.number_of_fats);
    data.max_entries = get_bytes(file_system, 0x011, 2);
}

// Add entry data to structure
void build_entry_data(void *file_system, int offset, int root_directory_entry_offset) {
    // Get date created information of entry
    entry_data.create_date = get_bytes(file_system, offset + root_directory_entry_offset + 0x10, 2);
    entry_data.day = entry_data.create_date & 0b11111;
    entry_data.month = (entry_data.create_date >> 5) & 0b1111;
    entry_data.year = (entry_data.create_date >> 9) & 0b1111111;
    entry_data.year += 1980;

    // Get time created information of entry
    entry_data.create_time = get_bytes(file_system, offset + root_directory_entry_offset + 0x0E, 2);
    // We only get 0-29 seconds, so multiply by 2 to get a full 60
    entry_data.seconds = entry_data.create_time & 0b11111;
    entry_data.seconds *= 2;
    entry_data.minutes = (entry_data.create_time >> 5) & 0b111111;
    entry_data.hours = (entry_data.create_time >> 11) & 0b11111;
    entry_data.ms = get_bytes(file_system, offset + root_directory_entry_offset + 0x0D, 1);
    // Add another second for odd seconds since the ms range is 0-199
    if (entry_data.ms >= 100) {
        entry_data.seconds += 1;
        entry_data.ms -= 100;
    }
    entry_data.ms *= 10;

    // Get date accessed information of entry
    entry_data.access_date = get_bytes(file_system, offset + root_directory_entry_offset + 0x12, 2);
    entry_data.access_day = entry_data.access_date & 0b11111;
    entry_data.access_month = (entry_data.access_date >> 5) & 0b1111;
    entry_data.access_year = (entry_data.access_date >> 9) & 0b111111;
    // Year runs from 0-127, so start at year 1980
    entry_data.access_year += 1980;

    // Get date modified information of entry
    entry_data.modify_date = get_bytes(file_system, offset + root_directory_entry_offset + 0x18, 2);
    entry_data.modify_day = entry_data.modify_date & 0b11111;
    entry_data.modify_month = (entry_data.modify_date >> 5) & 0b1111;
    entry_data.modify_year = (entry_data.modify_date >> 9) & 0b1111111;
    // Year runs from 0-127, so start at year 1980
    entry_data.modify_year += 1980;

    // Get time modified information of entry
    entry_data.modify_time = get_bytes(file_system, offset + root_directory_entry_offset + 0x16, 2);
    entry_data.modify_seconds = entry_data.modify_time & 0b11111;
    // We only get 0-29 seconds, so multiply by 2 to get a full 60
    entry_data.modify_seconds *= 2;
    entry_data.modify_minutes = (entry_data.modify_time >> 5) & 0b111111;
    entry_data.modify_hours = (entry_data.modify_time >> 11) & 0b11111;
    entry_data.modify_ms = 0;
}

// Print out value at a given size and location
void test_mmap(void *file_system) {
    char type;
    int address;
    // Read type and location from stdin
    while (scanf("%c %d\n", &type, &address) > 0) {
        switch (type) {
            case 'c':
                printf("%c\n", (char)get_bytes(file_system, address, 1));
                break;
            case 'b':
                printf("%02x\n", get_bytes(file_system, address, 1));
                break;
            case 's':
                printf("%d\n", get_bytes(file_system, address, 2));
                break;
            case 'w':
                printf("%04x\n", get_bytes(file_system, address, 2));
                break;
            case 'i':
                printf("%d\n", get_bytes(file_system, address, 4));
                break;
            case 'u':
                printf("%08x\n", get_bytes(file_system, address, 4));
                break;
        }
    }
}

// Print out boot sector information from given image
void test_boot_sector(void *file_system) {
    printf("OEM: ");
    // Print each letter of the OEM
    for (int i = 0; i < 8; i++) {
        printf("%c", get_bytes(file_system, 0x003 + i, 1));
    }
    printf("\n");
    printf("Bytes per sector: %d\n", data.bytes_per_sector);
    printf("Sectors per cluster: %d\n", data.sectors_per_cluster);
    printf("Reserved sectors: %d\n", data.reserved_sectors);
    printf("Num FATs: %d\n", data.number_of_fats);
    printf("Max root directory entries: %d\n", data.max_root_directory_entries);
    printf("Num logical sectors: %d\n", data.num_logical_sectors);
    printf("Media Descriptor: %x\n", data.media_descriptor);
    printf("Sectors per FAT: %d\n", data.sectors_per_fat);
}

// Print out root directory entry information
void test_directory_entry(void *file_system, int entry) {
    // Get entry starting byte
    int root_directory_entry_offset = entry * 32;

    build_entry_data(file_system, data.root_directory_start, root_directory_entry_offset);

    // Determine if the entry is empty
    if (get_bytes(file_system, data.root_directory_start + root_directory_entry_offset, 8) == 0) {
        printf("Empty entry\n");
    } else {
        // Determine if entry was previously erased
        if (get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x00 + 0, 1) == 0xE5) {
            printf("Previously erased entry\n");
            printf("Name: ");
            printf("?");
        } else {
            printf("Name: ");
            printf("%c", get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x00 + 0, 1));
        }
        // Get file name
        for (int i = 1; i < 8; i++) {
            printf("%c", get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x00 + i, 1));
        }
        printf(".");
        // Get file extension
        for (int i = 0; i < 3; i++) {
            printf("%c", get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x08 + i, 1));
        }
        printf("\n");

        // Determine file attributes
        int tmp = get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x0B, 1);
        char *file_attributes;
        if (tmp == 0x20) {
            file_attributes = "archive ";
        } else if (tmp == 0x10) {
            file_attributes = "subdir ";
        } else if (tmp == 34) {
            file_attributes = "Hidden archive ";
        } else if (tmp == 0x08) {
            file_attributes = "Vol. label ";
        } else if (tmp == 36) {
            file_attributes = "Sys archive ";
        } else if (tmp == 33) {
            file_attributes = "RO archive ";
        } else if (tmp == 15) {
            file_attributes = "RO Hidden Sys Vol. label ";
        } else {
            file_attributes = "";
        }

        // Print root directory entry information
        printf("File Attributes: %s\n", file_attributes);
        printf("Create time: %02d/%02d/%02d %02d:%02d:%02d.%03d\n", entry_data.year, entry_data.month, entry_data.day, entry_data.hours, entry_data.minutes, entry_data.seconds, entry_data.ms);
        printf("Access date: %02d/%02d/%02d\n", entry_data.access_year, entry_data.access_month, entry_data.access_day);
        printf("Extended attributes: %d\n", get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x14, 2));
        printf("Modify time: %02d/%02d/%02d %02d:%02d:%02d.%03d\n", entry_data.modify_year, entry_data.modify_month, entry_data.modify_day, entry_data.modify_hours, entry_data.modify_minutes, entry_data.modify_seconds, entry_data.modify_ms);
        printf("Start cluster: %d\n", get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x1A, 2));
        printf("Bytes: %d\n", get_bytes(file_system, data.root_directory_start + root_directory_entry_offset + 0x1C, 4));
    }
}

// Print cluster linked list
void test_file_clusters(void *file_system, int cluster) {
    // Clusters 0 and 1 are reserved for the FAT id and EOF
    if (cluster < 2) {
        printf("EOF\n");
        return;
    }

    // Start at given cluster
    int tmp = cluster;
    // Print linked list of clusters
    while (tmp != 0xFFFF && tmp > 1) {
        printf("%d", tmp);
        printf(" -> ");
        // Jump to the next node in linked list
        tmp = get_bytes(file_system, data.fat_start + tmp * 2, 2);
    }
    printf("EOF\n");
}


// Finds file entries by file name and prints file information
void test_file_name(void *file_system, char *filename) {
    int offset = data.root_directory_start;

    // Split filename by '/' to look in each directory
    char *token = strtok(filename, "/");
    while (token != NULL) {
        for (int i = 0; i < data.max_entries * 32; i += 32) {
            // Empty entry
            if (get_bytes(file_system, offset + i + 0x00, 1) == 0) {
                break;
            }
            char name[100] = "";
            // Get file name
            strncat(name, file_system + offset + i, 8);
            // Trim trailing whitespace
            int idx;
            idx = strlen(name) - 1;
            while (name[idx] == ' ' && idx > 0) idx--;
            name[++idx] = '\0';
            // Don't concatenate a '.' if there is no extension
            if (get_bytes(file_system, offset + i + 0x08, 1) == ' ') {
                ;
            } else {
                // Get file extension
                strcat(name, ".");
                strncat(name, file_system + offset + i + 0x08, 3);
            }

            if (strcmp(token, name) == 0) {
                int root_directory_entry_offset = i;

                // Determine file attributes
                int tmp = get_bytes(file_system, offset + root_directory_entry_offset + 0x0B, 1);
                char *file_attributes;
                if (tmp == 32) {
                    file_attributes = "archive ";
                } else if (tmp == 16) {
                    file_attributes = "subdir ";
                } else {
                    file_attributes = "";
                }

                // If the current entry is a directory, reset to the new cluster offset
                if (strcmp(file_attributes, "subdir ") == 0) {
                    int start_cluster = get_bytes(file_system, offset + root_directory_entry_offset + 0x1A, 2);
                    offset = ((data.bytes_per_sector * data.sectors_per_cluster) * (start_cluster - 2)) + ((32 * data.max_entries) + data.root_directory_start);
                    break;
                }

                build_entry_data(file_system, offset, root_directory_entry_offset);

                // Empty entry
                if (get_bytes(file_system, offset + root_directory_entry_offset, 8) == 0) {
                    printf("Empty entry\n");
                } else {
                    // Determine if entry was previously erased
                    if (get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + 0, 1) == 0xE5) {
                        printf("Previously erased entry\n");
                        printf("Name: ");
                        printf("?");
                    } else {
                        printf("Name: ");
                        printf("%c", get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + 0, 1));
                    }
                    // Get file name
                    for (int i = 1; i < 8; i++) {
                        printf("%c", get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + i, 1));
                    }
                    printf(".");
                    // Get file extension
                    for (int i = 0; i < 3; i++) {
                        printf("%c", get_bytes(file_system, offset + root_directory_entry_offset + 0x08 + i, 1));
                    }
                    printf("\n");

                    // Determine file attributes
                    int tmp = get_bytes(file_system, offset + root_directory_entry_offset + 0x0B, 1);
                    char *file_attributes;
                    if (tmp == 32) {
                        file_attributes = "archive ";
                    } else if (tmp == 16) {
                        file_attributes = "subdir ";
                    } else {
                        file_attributes = "";
                    }

                    // Print entry information
                    printf("File Attributes: %s\n", file_attributes);
                    printf("Create time: %02d/%02d/%02d %02d:%02d:%02d.%03d\n", entry_data.year, entry_data.month, entry_data.day, entry_data.hours, entry_data.minutes, entry_data.seconds, entry_data.ms);
                    printf("Access date: %02d/%02d/%02d\n", entry_data.access_year, entry_data.access_month, entry_data.access_day);
                    printf("Extended attributes: %d\n", get_bytes(file_system, offset + root_directory_entry_offset + 0x14, 2));
                    printf("Modify time: %02d/%02d/%02d %02d:%02d:%02d.%03d\n", entry_data.modify_year, entry_data.modify_month, entry_data.modify_day, entry_data.modify_hours, entry_data.modify_minutes, entry_data.modify_seconds, entry_data.modify_ms);
                    printf("Start cluster: %d\n", get_bytes(file_system, offset + root_directory_entry_offset + 0x1A, 2));
                    printf("Bytes: %d\n", get_bytes(file_system, offset + root_directory_entry_offset + 0x1C, 4));
                }
            }
        }
        // Iterate to next subdirectory
        token = strtok(NULL, "/");
    }
}

// Print out the contents of a given filename
void test_file_contents(void *file_system, char *filename) {
    int offset = data.root_directory_start;

    // Split filename by '/' to look in each directory
    char *token = strtok(filename, "/");
    while (token != NULL) {
        for (int i = 0; i < data.max_entries * 32; i += 32) {
            // Empty entry
            if (get_bytes(file_system, offset + i + 0x00, 1) == 0) {
                break;
            }
            char name[100] = "";
            // Get file name
            strncat(name, file_system + offset + i, 8);
            // Trim trailing whitespace
            int idx;
            idx = strlen(name) - 1;
            while (name[idx] == ' ' && idx > 0) idx--;
            name[++idx] = '\0';
            // Don't concatenate a '.' if there is no extension
            if (get_bytes(file_system, offset + i + 0x08, 1) == ' ') {
                ;
            } else {
                // Get file extension
                strcat(name, ".");
                strncat(name, file_system + offset + i + 0x08, 3);
            }
            if (strcmp(token, name) == 0) {
                int root_directory_entry_offset = i;

                // Determine file attributes
                int tmp = get_bytes(file_system, offset + root_directory_entry_offset + 0x0B, 1);
                char *file_attributes;
                if (tmp == 32) {
                    file_attributes = "archive ";
                } else if (tmp == 16) {
                    file_attributes = "subdir ";
                } else {
                    file_attributes = "";
                }

                int start_cluster = get_bytes(file_system, offset + root_directory_entry_offset + 0x1A, 2);
                // If the current entry is a directory, reset to the new cluster offset
                if (strcmp(file_attributes, "subdir ") == 0) {
                    offset = ((data.bytes_per_sector * data.sectors_per_cluster) * (start_cluster - 2)) + ((32 * data.max_entries) + data.root_directory_start);
                    break;
                } else {
                    // Print out file contents
                    int filesize = get_bytes(file_system, offset + root_directory_entry_offset + 0x1C, 4);
                    int file_start_offset = ((data.bytes_per_sector * data.sectors_per_cluster) * (start_cluster - 2)) + ((32 * data.max_entries) + data.root_directory_start);
                    for (int i = 0; i < filesize; i++) {
                        printf("%c", get_bytes(file_system, file_start_offset + i, 1));
                    }
                }
            }
        }
        // Iterate to next subdirectory
        token = strtok(NULL, "/");
    }
}

// Search file system for all possible attributes/statistics
void search_fs(void *file_system, int offset, int *num_root_dir_files, int *num_files, int *num_dirs, char curr_name[100], char file_name[100], int *max_file_size, int *size_of_files, int *curr_level, int *max_level) {
    // Increase directory level since we recursed into a directory
    (*curr_level)++;
    if (*curr_level > *max_level) {
        *max_level = *curr_level;
    }

    // Loop through current directory
    for (int i = 0; i < data.max_entries * 32; i += 32) {
        // Empty entry, doesn't count for total numbers
        if (get_bytes(file_system, offset + i + 0x00, 1) == 0 || get_bytes(file_system, offset + i + 0x00 + 0, 1) == 0x2E || get_bytes(file_system, offset + i + 0x00 + 0, 1) == 0xE5) {
            break;
        }

        int root_directory_entry_offset = i;

        // Get file name
        strcat(curr_name, "/");
        strncat(curr_name, file_system + offset + i, 8);

        // Trim trailing whitespace
        int idx;
        idx = strlen(curr_name) - 1;
        while ((curr_name)[idx] == ' ' && idx > 0) idx--;
        (curr_name)[++idx] = '\0';

        // Don't concatenate a '.' if there is no extension
        if (get_bytes(file_system, offset + i + 0x08, 1) == ' ') {
            ;
        } else {
            // Get file extension
            strcat(curr_name, ".");
            strncat(curr_name, file_system + offset + i + 0x08, 3);
        }

        // Determine file attributes
        int tmp = get_bytes(file_system, offset + root_directory_entry_offset + 0x0B, 1);
        if (tmp == 0x20 && !(get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + 0, 1) == 0xE5)) {
            (*num_files)++;
            int curr_file_size = get_bytes(file_system, offset + root_directory_entry_offset + 0x1C, 4);
            if (curr_file_size > *max_file_size) {
                *max_file_size = curr_file_size;
                strcpy(file_name, curr_name);
            }
            *size_of_files += get_bytes(file_system, offset + root_directory_entry_offset + 0x1C, 4);
        } else if (tmp == 0x10 && !(get_bytes(file_system, offset + i + 0x00, 1) == 0x2E) && !(get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + 0, 1) == 0xE5)) {
            (*num_dirs)++;
            // Calculate the starting offset for next directory
            int start_cluster = get_bytes(file_system, offset + root_directory_entry_offset + 0x1A, 2);
            int jump = ((data.bytes_per_sector * data.sectors_per_cluster) * (start_cluster - 2)) + ((32 * data.max_entries) + data.root_directory_start);
            search_fs(file_system, jump, num_root_dir_files, num_files, num_dirs, curr_name, file_name, max_file_size, size_of_files, curr_level, max_level);
            // After recursing through a directory, travel back up
            (*curr_level)--;
        }
    }
}

// Prints the number of files and directories in the given file system
void get_stats(void *file_system, char mode) {
    int num_root_dir_files = 0;
    int num_files = 0;
    int num_dirs = 0;

    int curr_file_size = 0;
    int max_file_size = 0;
    char file_name[100] = "";

    int capacity = 0;
    int active_entry_count = 0;
    int all_space = 0;
    int size_of_files = 0;
    int unused_all_space = 0;
    int unall_space = 0;

    // Root level is the first level
    int curr_level = 1;
    int max_level = 1;

    char *file_path = "";
    int start_cluster = 0;

    char *oldest_file_name = "";

    int offset = data.root_directory_start;

    // Loop through the root directory
    for (int i = 0; i < data.max_entries * 32; i += 32) {
        // Empty entry, doesn't count for total numbers
        if (get_bytes(file_system, offset + i + 0x00, 1) == 0) {
            break;
        }

        int root_directory_entry_offset = i;

        // Get file name
        char curr_name[100] = "";
        strcat(curr_name, "/");
        strncat(curr_name, file_system + offset + i, 8);

        // Trim trailing whitespace
        int idx;
        idx = strlen(curr_name) - 1;
        while (curr_name[idx] == ' ' && idx > 0) idx--;
        curr_name[++idx] = '\0';

        // Don't concatenate a '.' if there is no extension
        if (get_bytes(file_system, offset + i + 0x08, 1) == ' ') {
            ;
        } else {
            // Get file extension
            strcat(curr_name, ".");
            strncat(curr_name, file_system + offset + i + 0x08, 3);
        }

        // Determine file attributes
        int tmp = get_bytes(file_system, offset + root_directory_entry_offset + 0x0B, 1);
        if (tmp == 0x20 && !(get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + 0, 1) == 0xE5)) {
            num_root_dir_files++;
            num_files++;
            size_of_files += get_bytes(file_system, offset + root_directory_entry_offset + 0x1C, 4);
            curr_file_size = get_bytes(file_system, offset + root_directory_entry_offset + 0x1C, 4);
            if (curr_file_size > max_file_size) {
                max_file_size = curr_file_size;
                strcpy(file_name, curr_name);
            }
        } else if (tmp == 0x10 && !(get_bytes(file_system, offset + root_directory_entry_offset + 0x00 + 0, 1) == 0xE5)) {
            num_dirs++;
            // Calculate the starting offset for next directory
            int start_cluster = get_bytes(file_system, offset + root_directory_entry_offset + 0x1A, 2);
            int jump = ((data.bytes_per_sector * data.sectors_per_cluster) * (start_cluster - 2)) + ((32 * data.max_entries) + data.root_directory_start);
            search_fs(file_system, jump, &num_root_dir_files, &num_files, &num_dirs, curr_name, file_name, &max_file_size, &size_of_files, &curr_level, &max_level);
        }
    }

    if (mode == 'e') {
        printf("Number of files in root directory: %d\n", num_root_dir_files);
        printf("Number of files in the file system: %d\n", num_files);
        printf("Number of directories in the file system: %d\n", num_dirs);
    } else if (mode == 's') {
        // Calculate FAT table start and size to get active entries
        for (int i = data.fat_start + 4; i < data.fat_size; i += 2) {
            if (!(get_bytes(file_system, i, 2) == 0)) active_entry_count++;
        }

        // Get number of logical sectors
        int tmp = data.num_logical_sectors;
        if (tmp == 0) tmp = get_bytes(file_system, 0x020, 4);

        capacity = data.bytes_per_sector * tmp;
        all_space = active_entry_count * (data.bytes_per_sector * data.sectors_per_cluster);
        unused_all_space = all_space - size_of_files;
        unall_space = capacity - all_space;

        printf("Total capacity of the file system: %d\n", capacity);
        printf("Total allocated space: %d\n", all_space);
        printf("Total size of files: %d\n", size_of_files);
        printf("Unused, but allocated, space (for files): %d\n", unused_all_space);
        printf("Unallocated space: %d\n", unall_space);
    } else if (mode == 'l') {
        printf("Largest file (%d bytes): %s\n", max_file_size, file_name);
    } else if (mode == 'k') {
        printf("Path to file with cookie: %s\n", file_path);
        printf("Starting cluster for file with cookie: %d\n", start_cluster);
    } else if (mode == 'u') {
        printf("Directory hierarchy levels: %d\n", max_level);
    } else if (mode == 'f') {
        printf("Oldest file: %s\n", oldest_file_name);
    }
}

// Prints out all of the file system data of a given file system
void output_fs_data(void *file_system) {
    get_stats(file_system, 'e');
    get_stats(file_system, 's');
    get_stats(file_system, 'l');
    get_stats(file_system, 'k');
    get_stats(file_system, 'u');
    get_stats(file_system, 'f');
}

// TODO: Actually implement
void write_fs_data(void *file_system) {

}

int main(int argc, char **argv) {
    // Possible arguments to the program
    static struct option long_options[] = {
          {"image", required_argument, 0, 'i'},
          {"test-mmap", no_argument, 0, 'm'},
          {"test-boot-sector", no_argument, 0, 'b'},
          {"test-directory-entry", required_argument, 0, 'd'},
          {"test-file-clusters", required_argument, 0, 'c'},
          {"invalid-image", no_argument, 0, 'v'},
          {"test-file-name", required_argument, 0, 'n'},
          {"test-file-contents", required_argument, 0, 'o'},
          {"test-num-entries", no_argument, 0, 'e'},
          {"test-space-usage", no_argument, 0, 's'},
          {"test-largest-file", no_argument, 0, 'l'},
          {"test-cookie", no_argument, 0, 'k'},
          {"test-num-dir-levels", no_argument, 0, 'u'},
          {"test-oldest-file", no_argument, 0, 'f'},
          {"output-fs-data", no_argument, 0, 'a'},
          {"write-fs-data", no_argument, 0, 'w'},
          {0, 0, 0, 0}
    };

    // Read given arguments into variables
    int option_index = 0;
    char *image;
    int entry;
    int cluster;
    int c;
    char mode;
    char *filename;
    while ((c = getopt_long(argc, argv, "mbveslkufawd:c:i:n:o:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'i':
                image = optarg;
                break;
            case 'd':
                entry = atoi(optarg);
                mode = c;
                break;
            case 'c':
                cluster = atoi(optarg);
                mode = c;
                break;
            case 'v':
                break;
            case 'b':
            case 'm':
            case 'n':
            case 'o':
                mode = c;
                filename = optarg;
                break;
            case 'e':
            case 's':
            case 'l':
            case 'k':
            case 'u':
            case 'f':
            case 'a':
            case 'w':
                mode = c;
                break;
            default:
                printf("Incorrect arguments\n");
                break;
        };
    }

    // mmap() the given image to memory
    int fd = open(image, O_RDONLY, 0);
    struct stat st;
    stat(image, &st);
    int size = st.st_size;
    void *file_system = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    build_fs_data(file_system);

    // Test filesystem
    switch (mode) {
        case 'm':
            test_mmap(file_system);
            break;
        case 'b':
            test_boot_sector(file_system);
            break;
        case 'd':
            test_directory_entry(file_system, entry);
            break;
        case 'c':
            test_file_clusters(file_system, cluster);
            break;
        case 'n':
            test_file_name(file_system, filename);
            break;
        case 'o':
            test_file_contents(file_system, filename);
            break;
        case 'e':
        case 's':
        case 'l':
        case 'k':
        case 'u':
        case 'f':
            get_stats(file_system, mode);
            break;
        case 'a':
            output_fs_data(file_system);
            break;
        case 'w':
            write_fs_data(file_system);
            break;
        default:
            break;
    }

    return 0;
}
