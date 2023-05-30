#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 8

int isAddressInRange(unsigned long address, unsigned long start, unsigned long end) {
    return (address >= start && address < end);
}
// function to parse the maps file to see if a given virtual memory is among the virtual memory space of a process
int isAddressInMemoryRange(unsigned long va, int pid) {
    char mapsFilePath[20];
    snprintf(mapsFilePath, sizeof(mapsFilePath), "/proc/%d/maps", pid);

    FILE *mapsFile = fopen(mapsFilePath, "r");
    if (mapsFile == NULL) {
        perror("Failed to open the maps file");
        return 1;
    }
    int buffer_size = 256;
    char buffer[buffer_size];

    while (fgets(buffer, sizeof(buffer), mapsFile) != NULL) {
        char *addresses = strtok(buffer, " ");

        char *addressStart = strtok(addresses, "-");
        char *addressEnd = strtok(NULL, "-");
        
        unsigned long start = strtoul(addressStart, NULL, 16);
        unsigned long end = strtoul(addressEnd, NULL, 16);

        if (isAddressInRange(va, start, end)) {
            fclose(mapsFile);
            return 1;
        }
    }

    fclose(mapsFile);
    return 0;
}

// Function to read the contents of a file
// The given index is multiplied by 8 (bytes) to determine the byte offset
unsigned long read_file(const char *filename, unsigned long index)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
    }
    // Move the file pointer to the desired offset
    unsigned long offset = index * sizeof(unsigned long);
    if (lseek(fd, offset, SEEK_SET) == -1)
    {
        perror("Error seeking file");
        close(fd);
        return 1;
    }
    unsigned long value = 0;
    if (read(fd, &value, sizeof(unsigned long)) == -1)
    {
        perror("Error reading file");
        close(fd);
        return 1;
    }

    close(fd);
    return value;
}

// Function implementation for -frameinfo option
void process_frameinfo(unsigned long pfn)
{
    unsigned long pageFlags = read_file("/proc/kpageflags", pfn);
    unsigned long pageCounts = read_file("/proc/kpagecount", pfn);

    printf("Mapping count: %lu\n", pageCounts);

    // Print labels
    const char *labels[] = {
        "LOCKED", "ERROR", "REFERENCED", "UPTODATE", "DIRTY",
        "LRU", "ACTIVE", "SLAB", "WRITEBACK", "RECLAIM",
        "BUDDY", "MMAP", "ANON", "SWAPCACHE", "SWAPBACKED",
        "COMPOUND_HEAD", "COMPOUND_TAIL", "HUGE", "UNEVICTABLE", "HWPOISON",
        "NOPAGE", "KSM", "THP", "BALLOON", "ZERO_PAGE",
        "IDLE"};
    for (int i = 0; i < 26; i++)
    {
        if (i % 5 == 0)
        {
            printf("\n");
        }
        printf("%02d: %-15s", i, labels[i]);
    }

    printf("\n\n");

    // Print the flag values
    printf("%-13s", "FRAME#");
    for (int i = 0; i < 26; i++)
    {
        printf("%02d ", i);
    }
    printf("\n0x%09lx  ", pfn);
    for (int i = 0; i < 26; i++)
    {
        printf("%lu  ", (pageFlags & 1));
        pageFlags = pageFlags >> 1;
    }
    printf("\n");
}

// // Function to find the total virtual memory size from /proc/PID/maps
// unsigned long get_virtual_memory_size(int pid) {
//     char filename[128];
//     snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);

//     unsigned long mapsContent = read_file(filename, 0);
//     if (mapsContent == NULL)
//         return 0;

//     unsigned long totalSize = 0

//     // Iterate over the lines in the maps file
//     char* line = strtok(mapsContent, "\n");
//     while (line != NULL) {
//         // Parse the line and extract the start and end addresses
//         unsigned long start, end;
//         sscanf(line, "%lx-%lx", &start, &end);

//         // Calculate the size of the memory region
//         unsigned long size = end - start;

//         // Add the size to the total
//         totalSize += size;

//         // Move to the next line
//         line = strtok(NULL, "\n");
//     }

//     free(mapsContent);
//     return totalSize / 1024; // Convert to KB
// }

// // Function to find the physical memory sizes from /proc/kpagecount
// void get_physical_memory_sizes(int pid, unsigned long* exclusiveSize, unsigned long* inclusiveSize) {
//     char filename[128];
//     snprintf(filename, sizeof(filename), "/proc/%d/kpagecount", pid);

//     // Calculate offsets based on the size of unsigned long (8 bytes)
//     unsigned long exclusiveOffset = pid * 8;
//     unsigned long inclusiveOffset = pid * 8 + 8;

//     *exclusiveSize = read_file(filename, exclusiveOffset) * 4; // Each page is 4KB
//     *inclusiveSize = read_file(filename, inclusiveOffset) * 4; // Each page is 4KB
// }

// void process_memused(int pid) {
//     // Calculate virtual memory size
//     unsigned long virtualMemorySize = get_virtual_memory_size(pid);
//     // Calculate physical memory sizes
//     unsigned long exclusiveMemorySize, inclusiveMemorySize;
//     get_physical_memory_sizes(pid, &exclusiveMemorySize, &inclusiveMemorySize);
//     // Print the results
//     printf("Virtual Memory Size (KB): %lu\n", virtualMemorySize);
//     printf("Physical Memory (Exclusive) Size (KB): %lu\n", exclusiveMemorySize);
//     printf("Physical Memory (Inclusive) Size (KB): %lu\n", inclusiveMemorySize);
//     // Calculate physical memory sizes
//     get_physical_memory_sizes(pid, &exclusiveMemorySize, &inclusiveMemorySize);
//     // Function implementation for -memused option
//     printf("Processing -memused: %d\n", pid);
// }

// Given a 64-bit pagemap_entry, returns
//      1: if page present in memory
//      0: if page swapped out
//     -1: if page not used
int check_page(unsigned long pagemap_entry, int pid, unsigned long va) {
    // page not used
    if (!isAddressInMemoryRange(va, pid))
        return -1;

    if (pagemap_entry & (1UL << 63)) {
        // Page present in memory
        return 1;
    }

    // page not in memory
    return 0;
}

// printFlag = 0: print va and pa
// printFlag = 1: print vpn and pfn
void process_mapva(int pid, unsigned long va, int printFlag)
{
    // Function implementation for -mapva option
    unsigned long vpn = va >> 12;
    vpn = (vpn << 16) >> 16;

    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/pagemap", pid);
    unsigned long pagemap_entry = read_file(filename, vpn);
    int check = check_page(pagemap_entry, pid, va);

    if (printFlag == 0) {
        printf("virtual address = 0x%016lx ", va);
    }
    else {
        printf("mapping: vpn=0x%09lx ", vpn);
    }
    
    if (check == 1) {
        unsigned long page_offset = (va << 52) >> 52;
        unsigned long pa = (pagemap_entry << 12) + page_offset;
        if (printFlag == 0) {
            printf("physical address: 0x%016lx\n", pa);
        }
        else {
            unsigned long pfn = (pagemap_entry << 9) >> 9;
            printf("pfn=0x%09lx\n", pfn);
        }
    }
    else if (check == 0) {
        printf("not-in-memory\n");
    }
    else {
        printf("unused\n");
    }
}

void process_pte(int pid, unsigned long va)
{
    // Function implementation for -mapva option
    unsigned long vpn = va >> 12;
    vpn = (vpn << 16) >> 16;

    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/pagemap", pid);
    unsigned long pagemap_entry = read_file(filename, vpn);
    int check = check_page(pagemap_entry, pid, va);

    unsigned long present = (pagemap_entry & (1UL << 63)) >> 63;
    unsigned long swapped = (pagemap_entry & (1UL << 62)) >> 62;
    unsigned long file_anon = (pagemap_entry & (1UL << 61)) >> 61;
    unsigned long exclusive = (pagemap_entry & (1UL << 56)) >> 56;
    unsigned long soft_dirty = (pagemap_entry & (1UL << 55)) >> 55;

    printf("[vaddr=0x%012lx, vpn=0x%09lx]: present=%lu, swapped=%lu, ", va, vpn, present, swapped);

    if (swapped == 1UL){
        unsigned long swap_type = (pagemap_entry >> 59) << 59;
        unsigned long swap_offset = ((pagemap_entry << 5) >> 15) << 15;
        printf("swap-type: 0x%05lx, swap-offset: 0x%050lx, ", swap_type, swap_offset);
    }

    printf("file-anon=%lu, exclusive=%lu, softdirty=%lu, ", file_anon, exclusive, soft_dirty);

    if (check == 1) {
        unsigned long page_offset = (va << 52) >> 52;
        unsigned long pfn = (pagemap_entry << 12) >> 12;
        unsigned long pa = (pagemap_entry << 12) + page_offset;
        printf("page_offset=0x%03lx, pfn=0x%09lx, physical-address=0x%016lx\n", page_offset, pfn, pa);
    }
    else if (check == 0) {
        printf("not-in-memory\n");
    }
    else {
        printf("unused\n");
    }
}

void process_maprange(int pid, unsigned long va1, unsigned long va2)
{
    va1 = (va1 >> 12) << 12;
    for (unsigned long addr = va1; addr < va2; addr += 4096) {
        process_mapva(pid, addr, 1);
    }
}

void process_mapall(int pid)
{
    // Function implementation for -mapall option
    printf("Processing -mapall: %d\n", pid);
}

void process_mapallin(int pid)
{
    // Function implementation for -mapallin option
    printf("Processing -mapallin: %d\n", pid);
}

void process_alltablesize(int pid)
{
    // Function implementation for -alltablesize option
    printf("Processing -alltablesize: %d\n", pid);
}

void printBinary(unsigned char byte)
{
    for (int i = 8 - 1; i >= 0; i--)
    {
        unsigned char mask = 1 << i;
        putchar((byte & mask) ? '1' : '0');
    }
}

void print_file()
{
    int fd = open("/proc/kpageflags", O_RDONLY); // Open the file in read-only mode

    if (fd == -1)
    {
        perror("Failed to open the file");
    }

    unsigned char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    off_t frameNumber = 0;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0 && frameNumber < 10)
    {
        printf("0x%llx: ", (long long)frameNumber);
        for (int i = 0; i < bytesRead; i++)
        {
            printBinary(buffer[i]);

            if ((i + 1) % (BUFFER_SIZE / 8) == 0)
                printf(" ");
        }

        printf("\n");
        frameNumber++;
    }

    if (bytesRead == -1)
    {
        perror("Failed to read the file");
        close(fd);
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Invalid number of arguments\n");
        return 1;
    }

    if (strcmp(argv[1], "-frameinfo") == 0)
    {
        if (argc != 3)
        {
            printf("Invalid number of arguments for -frameinfo\n");
            return 1;
        }

        unsigned long pfn = strtoul(argv[2], NULL, 0);
        process_frameinfo(pfn);
    }
    else if (strcmp(argv[1], "-memused") == 0)
    {
        if (argc < 4)
        {
            printf("Invalid number of arguments for -memused\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        // process_memused(pid);
    }
    else if (strcmp(argv[1], "-mapva") == 0)
    {
        if (argc < 4)
        {
            printf("Invalid number of arguments for -mapva\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        unsigned long va = strtoul(argv[3], NULL, 0);
        process_mapva(pid, va, 0);
    }
    else if (strcmp(argv[1], "-pte") == 0)
    {
        if (argc < 4)
        {
            printf("Invalid number of arguments for -pte\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        unsigned long va = strtoul(argv[3], NULL, 0);
        process_pte(pid, va);
    }
    else if (strcmp(argv[1], "-maprange") == 0)
    {
        if (argc < 5)
        {
            printf("Invalid number of arguments for -maprange\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        unsigned long va1 = strtoul(argv[3], NULL, 0);
        unsigned long va2 = strtoul(argv[4], NULL, 0);
        process_maprange(pid, va1, va2);
    }
    else if (strcmp(argv[1], "-mapall") == 0)
    {
        if (argc < 4)
        {
            printf("Invalid number of arguments for -mapall\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_mapall(pid);
    }
    else if (strcmp(argv[1], "-mapallin") == 0)
    {
        if (argc < 4)
        {
            printf("Invalid number of arguments for -mapallin\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_mapallin(pid);
    }
    else if (strcmp(argv[1], "-alltablesize") == 0)
    {
        if (argc < 4)
        {
            printf("Invalid number of arguments for -alltablesize\n");
            return 1;
        }
        int pid = atoi(argv[3]);
        process_alltablesize(pid);
    }
    else
    {
        printf("Invalid option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
