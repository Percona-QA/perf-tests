#define _GNU_SOURCE  // To enable O_DIRECT
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>

#define FILE_NAME "testfile"
#define BUFFER_SIZE 512

void test_sync(int rounds) {
    printf("Start testing %d rounds...\n", rounds);
    printf("getpagesize() = %d\n", getpagesize());

    // Open a file
    int fd = open(FILE_NAME, O_RDWR | O_CREAT | O_DIRECT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Extend the file size to the buffer size
    if (ftruncate(fd, BUFFER_SIZE) == -1) {
        perror("Error extending file");
        close(fd);
        exit(EXIT_FAILURE);
    }

#ifdef USE_MMAP
    const char* alloc_name="mmap";
    char *buffer = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("Error mapping file to memory");
        close(fd);
        exit(EXIT_FAILURE);
    }
#else
    // Allocate aligned buffer
    const char* alloc_name="posix_memalign";
    char *buffer;
    if (posix_memalign((void**)&buffer, getpagesize(), BUFFER_SIZE) != 0) {
        perror("Error allocating aligned buffer");
        close(fd);
        exit(EXIT_FAILURE);
    }
#endif
    memset(buffer, 0, BUFFER_SIZE);

#ifdef USE_MSYNC
    printf("using %s + msync\n", alloc_name);
#else
#ifndef USE_MMAP
    printf("using %s + write + fsync\n", alloc_name);
#else
    printf("using %s + fsync\n", alloc_name);
#endif
#endif

    int written = 0;
    for (int i = 0; i < rounds; ++i) {
        buffer[i % BUFFER_SIZE] = rand() % 256;
#ifdef USE_MSYNC
        msync(buffer, BUFFER_SIZE, MS_SYNC);  // Synchronize changes to the file
#else
#ifndef USE_MMAP
        lseek(fd, 0, SEEK_SET);
        ssize_t res = write(fd, buffer, BUFFER_SIZE);
        if (res == -1) {
            perror("Error writing to file");
            close(fd);
            exit(EXIT_FAILURE);
        }
        written += res;
#endif // USE_MMAP
        fsync(fd);
#endif // USE_MSYNC
    }

    printf("Written %d bytes\n", written);

#ifdef USE_MMAP
    // Unmap the memory
    if (munmap(buffer, BUFFER_SIZE) == -1) {
        perror("Error unmapping file from memory");
        close(fd);
        exit(EXIT_FAILURE);
    }
#else
    free(buffer);
#endif

    // Close opened file
    if (close(fd) == -1) {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }

    // Remove the file
    if (remove(FILE_NAME) == -1) {
        perror("Error removing file");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
      printf("Usage: time %s <rounds>\n", argv[0]);
      printf("rounds - a number of fsync calls\n");
      exit(EXIT_FAILURE);
    }

    srand((unsigned int)time(NULL)); // Seed for random number generation

    int rounds = atoi(argv[1]);
    test_sync(rounds);

    return 0;
}


