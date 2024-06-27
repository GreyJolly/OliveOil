#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "file_system.h"

#define MAXIMUM_SIZE 538656 // At one time, this was calculated to be the maximum size 

int main()
{
	printf("Started testing...\n");
	// Start timing tests
	clock_t beginningTime = clock();

    // Allocate a large enough memory block

    void *memory = malloc(MAXIMUM_SIZE);

    // Initialize the filesystem with the provided memory block
    FileSystem *fs = initializeFileSystem(memory, MAXIMUM_SIZE);

    
    createDir(fs, "dir1");
    createDir(fs, "dir2");
    createFile(fs, "file1.txt");
    createFile(fs, "file2.txt");

	FileHandle * fh = open(fs, "file1.txt");
	write(fs, fh, "Contents of file1.txt", strlen("Contents of file1.txt"));
	seek(fs, fh, 0, SEEK_BEGIN);

	char * string = read(fs, fh, strlen("Contents of file1.txt"));
	printf("Contents of first file:\n%s\n", string);
	free(string);
	close(fh);

    printf("Root directory listing:\n");
    listDir(fs);
    
    changeDir(fs, "dir1");
    createFile(fs, "file3.txt");
    printf("\nDirectory listing after changing to dir1:\n");
    listDir(fs);
    
    changeDir(fs, "/");
    eraseFile(fs, "file1.txt"); 	// Erase file1.txt
    eraseDir(fs, "dir2"); 	// Erase dir2
    printf("\nRoot directory listing after deletions:\n");
    listDir(fs);
    

	free(memory);
	// End timing tests
	clock_t endingTime = clock();
	printf("Ended testing.");
	printf("\nTime:\t%.3lfms\n", (double)(endingTime - beginningTime) / CLOCKS_PER_SEC * 1000);
}
