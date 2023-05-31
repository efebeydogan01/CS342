#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 8
#define PAGE_SIZE 4096

// necessary numbers for page table size calculation (alltablesize command)
#define FOURTH_LEVEL_MAX 134217728
#define THIRD_LEVEL_MAX 262144
#define SECOND_LEVEL_MAX 512
#define PAGE_TABLE_ENTRY_SIZE 512

int isAddressInRange(unsigned long address, unsigned long start, unsigned long end) {
    return (address >= start && address < end);
}
// function to parse the maps file to see if a given virtual memory is among the virtual memory space of a process
int isAddressInMemoryRange(unsigned long va, int pid) {
    char mapsFilePath[256];
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

// Function to find the total virtual memory size from /proc/PID/maps
unsigned long get_virtual_memory_size(int pid) {
    char file[128];
    snprintf(file, sizeof(file), "/proc/%d/maps", pid);

    FILE* maps = fopen(file, "r");
    if(maps == NULL) {
        perror("Error opening maps");
        return 0;
    }

    unsigned long size = 0;
    while(!feof(maps)) {
        char line[256];
        if(fgets(line, sizeof(line),maps) != NULL) {
            unsigned long start, end;
            if(sscanf(line, "%lx-%lx",&start, &end) == 2) {
                size += end - start;
            }
        }
    }
    fclose(maps);
    return size/1024;
}

// Function to get the physical memory sizes for a process
void get_physical_memory_sizes(int pid, unsigned long* exclusiveSize, unsigned long* totalSize) {
    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    unsigned long exclusivePages = 0;
    unsigned long totalPages = 0;
   
    int buffer_size = 256;
    char buffer[buffer_size];

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        char *addresses = strtok(buffer, " ");

        char *addressStart = strtok(addresses, "-");
        char *addressEnd = strtok(NULL, "-");
        
        unsigned long start = strtoul(addressStart, NULL, 16);
        unsigned long end = strtoul(addressEnd, NULL, 16);

        start = (start << 16) >> 16;
        end = (end << 16) >> 16;

        unsigned long startPage = start / PAGE_SIZE;
        unsigned long endPage = end / PAGE_SIZE;

        for (unsigned long page = startPage; page < endPage; ++page) {
            char pgMap[128];

            snprintf(pgMap, sizeof(pgMap), "/proc/%d/pagemap", pid);
            
            unsigned long pfn = read_file(pgMap, page);
            unsigned long firstBit = pfn & (1UL << 63);
            pfn = (pfn << 9) >> 9;

            if (firstBit == 0) {
                continue;
            }
            
            unsigned long mappingCount = read_file("/proc/kpagecount", pfn);
            if (mappingCount == 1){
                exclusivePages++;
            }
            totalPages++;
        }
    }

    fclose(file);
    *exclusiveSize = exclusivePages * 4;
    *totalSize = totalPages * 4;
}

void process_memused(int pid) {
    // Calculate virtual memory size
    unsigned long virtualMemorySize = get_virtual_memory_size(pid);

    // Calculate physical memory sizes
    unsigned long exclusiveMemorySize = 0;
    unsigned long inclusiveMemorySize = 0;
    get_physical_memory_sizes(pid, &exclusiveMemorySize, &inclusiveMemorySize);
    
    // Print the results
    printf("(pid=%d) memused: virtual = %lu KB, mappedonce = %lu KB, pmem_all = %lu KB\n", 
            pid, 
            virtualMemorySize, 
            exclusiveMemorySize, 
            inclusiveMemorySize);
}

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
    // Function implementation for -mapallin option
    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            perror("Error parsing line");
            fclose(file);
            return;
        }

        for (unsigned long va = start; va <= end; va += PAGE_SIZE) {
            unsigned long vpn = va >> 12;
            vpn = (vpn << 16) >> 16;

            char filename[128];
            snprintf(filename, sizeof(filename), "/proc/%d/pagemap", pid);
            unsigned long pagemap_entry = read_file(filename, vpn);

            if (pagemap_entry & (1UL << 63)) {
                unsigned long pfn = (pagemap_entry << 9) >> 9;
                printf("mapping vpn=0x%09lx , pfn=0x%09lx\n", vpn, pfn);
            } else {
                printf("mapping vpn=0x%09lx , not-in-memory \n", vpn);
            }
        }
    }
    fclose(file);
}

void process_mapallin(int pid)
{
    // Function implementation for -mapallin option
    char filename[128];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            perror("Error parsing line");
            fclose(file);
            return;
        }

        for (unsigned long va = start; va <= end; va += PAGE_SIZE) {
            unsigned long vpn = va >> 12;
            vpn = (vpn << 16) >> 16;

            char filename[128];
            snprintf(filename, sizeof(filename), "/proc/%d/pagemap", pid);
            unsigned long pagemap_entry = read_file(filename, vpn);

            if (pagemap_entry & (1UL << 63)) {
                unsigned long pfn = (pagemap_entry << 9) >> 9;
                printf("mapping vpn=0x%09lx , pfn=0x%09lx\n", vpn, pfn);
            }
        }
    }
    fclose(file);
}

int existsInArray(unsigned long el, unsigned long *arr, unsigned long size) {
    for (unsigned long i = 0; i < size; i++) {
        if (arr[i] == el)
            return 1;
    }
    return 0;
}

unsigned long* allocateArray(unsigned long initSize) {
    unsigned long* res = (unsigned long *) malloc(sizeof(unsigned long) * initSize);
    return res;
}

void insertArray(unsigned long el, unsigned long **arr, unsigned long *size, unsigned long *curMaxSize) {
    if (*curMaxSize <= *size) {
        (*curMaxSize) = (*curMaxSize) * 2;
        *arr = (unsigned long *) realloc(*arr, (*curMaxSize) * sizeof(unsigned long)); 
    }
    (*arr)[*size] = el;
    *size = (*size) + 1;
}

void process_alltablesize(int pid)
{
    // Function implementation for -alltablesize option
    unsigned long fourthLevelSize = 0, thirdLevelSize = 0, secondLevelSize = 0;
    unsigned long curMaxSize = 128, thirdMaxSize = 8, secondMaxSize = 4;
    unsigned long *fourthLevel = allocateArray(curMaxSize);
    unsigned long *thirdLevel = allocateArray(thirdMaxSize);
    unsigned long *secondLevel = allocateArray(secondMaxSize);

    // get table information for the 4th level
    char mapsFilePath[256];
    snprintf(mapsFilePath, sizeof(mapsFilePath), "/proc/%d/maps", pid);
    
    FILE *mapsFile = fopen(mapsFilePath, "r");
    if (mapsFile == NULL) {
        perror("Failed to open the maps file");
        return;
    }

    int buffer_size = 256;
    char buffer[buffer_size];

    // the amount of memory a 4th level page table maps
    unsigned long prev_start, prev_end, start, end;
    unsigned long line = 0;
    while (fgets(buffer, sizeof(buffer), mapsFile) != NULL) {
        char *addresses = strtok(buffer, " ");

        char *addressStart = strtok(addresses, "-");
        char *addressEnd = strtok(NULL, "-");
        
        start = strtoul(addressStart, NULL, 16) >> 21;
        end = (strtoul(addressEnd, NULL, 16) - 1) >> 21;

        // check if prev end and start match
        if (line == 0) {
            prev_start = start;
            prev_end = end;
        }
        else if (prev_end != start) {
            insertArray(prev_start, &fourthLevel, &fourthLevelSize, &curMaxSize);
            insertArray(prev_end, &fourthLevel, &fourthLevelSize, &curMaxSize);

            prev_start = start;
        }
        prev_end = end;
        line++;
    }
    if (fourthLevelSize > 0) {
        insertArray(prev_start, &fourthLevel, &fourthLevelSize, &curMaxSize);
        insertArray(prev_end, &fourthLevel, &fourthLevelSize, &curMaxSize);
    }

    fclose(mapsFile);

    unsigned long lvl1_count = 0, lvl2_count = 0, lvl3_count = 0, lvl4_count = 0;
    for (unsigned long i = 1; i < fourthLevelSize; i+=2) {
        lvl4_count += fourthLevel[i] - fourthLevel[i-1] + 1;

        start = fourthLevel[i-1] >> 9;
        end = fourthLevel[i] >> 9;
        if (i > 1 && start == thirdLevel[thirdLevelSize - 1]) {
            thirdLevel[thirdLevelSize - 1] = end;
        }
        else { 
            insertArray(start, &thirdLevel, &thirdLevelSize, &thirdMaxSize);
            insertArray(end, &thirdLevel, &thirdLevelSize, &thirdMaxSize);
        }
    }
    for (unsigned long i = 1; i < thirdLevelSize; i+=2) {
        unsigned long start, end;
        lvl3_count += thirdLevel[i] - thirdLevel[i-1] + 1;

        start = thirdLevel[i-1] >> 9;
        end = thirdLevel[i] >> 9;
        if (i > 1 && start == secondLevel[secondLevelSize - 1]) {
            secondLevel[secondLevelSize - 1] = end;
        }
        else { 
            insertArray(start, &secondLevel, &secondLevelSize, &secondMaxSize);
            insertArray(end, &secondLevel, &secondLevelSize, &secondMaxSize);
        }
    }
    for (unsigned long i = 1; i < secondLevelSize; i+=2) {
        lvl2_count += secondLevel[i] - secondLevel[i-1] + 1;
    }
    lvl1_count = secondLevelSize > 0;
    unsigned long totalTableCount = lvl1_count + lvl2_count + lvl3_count + lvl4_count;
    printf("(pid=%d) total memory occupied by 4-level page table: %lu KB (%lu tables)\n", pid, totalTableCount * 4, totalTableCount);
    printf("(pid=%d) number of page tables used: level1=%lu, level2=%lu, level3=%lu, level4=%lu\n", pid, lvl1_count, lvl2_count, lvl3_count, lvl4_count);
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
        if (argc < 3)
        {
            printf("Invalid number of arguments for -memused\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        process_memused(pid);
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
        if (argc < 3)
        {
            printf("Invalid number of arguments for -mapall\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        process_mapall(pid);
    }
    else if (strcmp(argv[1], "-mapallin") == 0)
    {
        if (argc < 3)
        {
            printf("Invalid number of arguments for -mapallin\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        process_mapallin(pid);
    }
    else if (strcmp(argv[1], "-alltablesize") == 0)
    {
        if (argc < 3)
        {
            printf("Invalid number of arguments for -alltablesize\n");
            return 1;
        }
        int pid = atoi(argv[2]);
        process_alltablesize(pid);
    }
    else
    {
        printf("Invalid option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
