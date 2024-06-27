#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "file_system.h"

int main()
{
	printf("Started testing...\n");
	// Start timing tests
	clock_t beginningTime = clock();

	FileSystem *fs = initializeFileSystem();
    
    createDir(fs, "dir1");
    createDir(fs, "dir2");
    createFile(fs, "file1.txt");
    createFile(fs, "file2.txt");

	FileHandle * fh = open(fs, "file1.txt");
	write(fs, fh, "Contents of file1.txt", strlen("Contents of file1.txt"));
	seek(fs, fh, 0, SEEK_BEGIN);

	printf("Contents of first file:\n%s\n", read(fs, fh, strlen("Contents of file1.txt")));

    printf("Root directory listing:\n");
    listDir(fs);
    
    changeDir(fs, "dir1");
    createFile(fs, "file3.txt");
    printf("\nDirectory listing after changing to dir1:\n");
    listDir(fs);
    
    changeDir(fs, "/");
    eraseFile(fs, &(fs->entries[2])); 	// Erase file1.txt
    eraseDir(fs, &(fs->entries[1])); 	// Erase dir2
    printf("\nRoot directory listing after deletions:\n");
    listDir(fs);
    
    free(fs->table);
    free(fs);
	// End timing tests
	clock_t endingTime = clock();
	printf("Ended testing.");
	printf("\nTime:\t%.3lfms\n", (double)(endingTime - beginningTime) / CLOCKS_PER_SEC * 1000);
}
