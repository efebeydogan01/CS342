#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

void process_frameinfo(int pfn) {
    // Function implementation for -frameinfo option
    printf("Processing -frameinfo: %d\n", pfn);
}

// Function to read the contents of a file
unsigned long read_file(const char* filename, unsigned long offset) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return 0;
    }

    // Move the file pointer to the desired offset
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking file");
        close(fd);
        return 0;
    }

    unsigned long value = 0;
    if (read(fd, &value, sizeof(unsigned long)) == -1) {
        perror("Error reading file");
        close(fd);
        return 0;
    }

    close(fd);
    return value;
}

// Function to find the total virtual memory size from /proc/PID/maps
unsigned long get_virtual_memory_size(int pid) {
    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);

    char* mapsContent = read_file(filename);
    if (mapsContent == NULL)
        return 0;

    unsigned long totalSize = 0;

    // Iterate over the lines in the maps file
    char* line = strtok(mapsContent, "\n");
    while (line != NULL) {
        // Parse the line and extract the start and end addresses
        unsigned long start, end;
        sscanf(line, "%lx-%lx", &start, &end);

        // Calculate the size of the memory region
        unsigned long size = end - start;

        // Add the size to the total
        totalSize += size;

        // Move to the next line
        line = strtok(NULL, "\n");
    }

    free(mapsContent);
    return totalSize / 1024; // Convert to KB
}

// Function to find the physical memory sizes from /proc/kpagecount
void get_physical_memory_sizes(int pid, unsigned long* exclusiveSize, unsigned long* inclusiveSize) {
    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/kpagecount", pid);

    // Calculate offsets based on the size of unsigned long (8 bytes)
    unsigned long exclusiveOffset = pid * 8;
    unsigned long inclusiveOffset = pid * 8 + 8;

    *exclusiveSize = read_file(filename, exclusiveOffset) * 4; // Each page is 4KB
    *inclusiveSize = read_file(filename, inclusiveOffset) * 4; // Each page is 4KB
}

void process_memused(int pid) {
    // Calculate virtual memory size
    unsigned long virtualMemorySize = get_virtual_memory_size(pid);
    // Calculate physical memory sizes
    unsigned long exclusiveMemorySize, inclusiveMemorySize;
    get_physical_memory_sizes(pid, &exclusiveMemorySize, &inclusiveMemorySize);
    // Print the results
    printf("Virtual Memory Size (KB): %lu\n", virtualMemorySize);
    printf("Physical Memory (Exclusive) Size (KB): %lu\n", exclusiveMemorySize);
    printf("Physical Memory (Inclusive) Size (KB): %lu\n", inclusiveMemorySize);
    // Calculate physical memory sizes
    unsigned long exclusiveMemorySize, inclusiveMemorySize;
    get_physical_memory_sizes(pid, &exclusiveMemorySize, &inclusiveMemorySize);
    // Function implementation for -memused option
    printf("Processing -memused: %d\n", pid);
}

void process_mapva(int pid, const char* va) {
    // Function implementation for -mapva option
    printf("Processing -mapva: %d, %s\n", pid, va);
}

void process_pte(int pid, const char* va) {
    // Function implementation for -pte option
    printf("Processing -pte: %d, %s\n", pid, va);
}

void process_maprange(int pid, const char* va1, const char* va2) {
    // Function implementation for -maprange option
    printf("Processing -maprange: %d, %s, %s\n", pid, va1, va2);
}

void process_mapall(int pid) {
    // Function implementation for -mapall option
    printf("Processing -mapall: %d\n", pid);
}

void process_mapallin(int pid) {
    // Function implementation for -mapallin option
    printf("Processing -mapallin: %d\n", pid);
}

void process_alltablesize(int pid) {
    // Function implementation for -alltablesize option
    printf("Processing -alltablesize: %d\n", pid);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Invalid number of arguments\n");
        return 1;
    }

    if (strcmp(argv[1], "-frameinfo") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for -frameinfo\n");
            return 1;
        }
        int pfn = atoi(argv[3]);
        process_frameinfo(pfn);
    } else if (strcmp(argv[1], "-memused") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for -memused\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_memused(pid);
    } else if (strcmp(argv[1], "-mapva") == 0) {
        if (argc < 5) {
            printf("Invalid number of arguments for -mapva\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        const char* va = argv[4];
        process_mapva(pid, va);
    } else if (strcmp(argv[1], "-pte") == 0) {
        if (argc < 5) {
            printf("Invalid number of arguments for -pte\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        const char* va = argv[4];
        process_pte(pid, va);
    } else if (strcmp(argv[1], "-maprange") == 0) {
        if (argc < 6) {
            printf("Invalid number of arguments for -maprange\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        const char* va1 = argv[4];
        const char* va2 = argv[5];
        process_maprange(pid, va1, va2);
    } else if (strcmp(argv[1], "-mapall") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for -mapall\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_mapall(pid);
    } else if (strcmp(argv[1], "-mapallin") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for -mapallin\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_mapallin(pid);
    } else if (strcmp(argv[1], "-alltablesize") == 0) {
        if (argc < 4) {
            printf("Invalid number of arguments for -alltablesize\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_alltablesize(pid);
    } else {
        printf("Invalid option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
