#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "file_system.h"

#define FILESYSTEM_SIZE 1000000000

#define TEST_PASS 1
#define TEST_FAIL 0

int testsRun = 0;
int testsPassed = 0;

// Function to convert sizes to readable formats
char *formatSize(size_t size)
{
	static char formattedSize[20];
	double readableSize = (double)size;

	if (size >= (1 << 30))
	{
		readableSize /= (1 << 30);
		snprintf(formattedSize, sizeof(formattedSize), "%.2f GB", readableSize);
	}
	else if (size >= (1 << 20))
	{
		readableSize /= (1 << 20);
		snprintf(formattedSize, sizeof(formattedSize), "%.2f MB", readableSize);
	}
	else if (size >= (1 << 10))
	{
		readableSize /= (1 << 10);
		snprintf(formattedSize, sizeof(formattedSize), "%.2f KB", readableSize);
	}
	else
	{
		snprintf(formattedSize, sizeof(formattedSize), "%lu bytes", size);
	}

	return formattedSize;
}

void assertTest(int condition, const char *testName)
{
	testsRun++;
	if (condition)
	{
		testsPassed++;
		printf("[PASS] %s\n", testName);
	}
	else
	{
		printf("[FAIL] %s\n", testName);
	}
}

void testFileSystem()
{

	// Allocate a large enough memory block
	void *memory = malloc(FILESYSTEM_SIZE);

	// Initialize the filesystem with the provided memory block
	FileSystem *fs = initializeFileSystem(memory, FILESYSTEM_SIZE);

	if (!fs)
	{
		perror("Failed to initialize file system");
		return;
	}

	printf("File system initialized.\n");
	printf("Total size: %s\n", formatSize(getTotalSize(fs)));
	printf("Occupied size: %s\n", formatSize(getOccupiedSize(fs)));

	// Create nested directories
	assertTest(createDir(fs, "dir1") == 0, "Create directory 'dir1' in root");
	assertTest(createDir(fs, "dir2") == 0, "Create directory 'dir2' in root");
	assertTest(changeDir(fs, "dir1") == 0, "Change to directory 'dir1'");
	assertTest(createDir(fs, "dir3") == 0, "Create directory 'dir3' inside 'dir1'");
	assertTest(changeDir(fs, "dir3") == 0, "Change to directory 'dir3' inside 'dir1'");
	assertTest(createDir(fs, "dir4") == 0, "Create directory 'dir4' inside 'dir3'");

	// Create some files
	assertTest(createFile(fs, "file3.txt") == 0, "Create file 'file3.txt' in dir4");
	assertTest(createFile(fs, "file4.txt") == 0, "Create file 'file4.txt' in dir4");

	assertTest(changeDir(fs, "/") == 0, "Change to root");

	assertTest(createFile(fs, "file1.txt") == 0, "Create file 'file1.txt' in root");
	assertTest(createFile(fs, "file2.txt") == 0, "Create file 'file2.txt' in root");

	// List current directory
	printf("Listing root directory:\n");
	listDir(fs);

	// Test writing to files
	FileHandle *fh1 = open(fs, "file1.txt");
	assertTest(fh1 != NULL, "Open file 'file1.txt'");
	if (fh1)
	{
		assertTest(write(fs, fh1, "Hello, World!", 13) == 13, "Write to 'file1.txt'");
		attributes attr;

		assertTest(getAttributes(fs, fh1, &attr) == 0, "Get attributes of 'file1.txt'");
		printf("Size of 'file1.txt': %s\n", formatSize(attr.size));
		
		close(fh1);
	}

	FileHandle *fh2 = open(fs, "file2.txt");
	assertTest(fh2 != NULL, "Open file 'file2.txt'");
	if (fh2)
	{
		assertTest(write(fs, fh2, "Another test.", 13) == 13, "Write to 'file2.txt'");
		close(fh2);
	}

	printf("Occupied size: %s\n", formatSize(getOccupiedSize(fs)));

	// Test reading from files
	fh1 = open(fs, "file1.txt");
	if (fh1)
	{
		char *data = read(fs, fh1, 13);
		assertTest(data != NULL && strcmp(data, "Hello, World!") == 0, "Read from 'file1.txt'");
		free(data);
		close(fh1);
	}

	fh2 = open(fs, "file2.txt");
	if (fh2)
	{
		char *data = read(fs, fh2, 13);
		assertTest(data != NULL && strcmp(data, "Another test.") == 0, "Read from 'file2.txt'");
		free(data);
		close(fh2);
	}

	// Erase nested directories and files
	assertTest(eraseFile(fs, "file1.txt") == 0, "Erase file 'file1.txt'");
	assertTest(eraseFile(fs, "file2.txt") == 0, "Erase file 'file2.txt'");
	assertTest(eraseDir(fs, "dir1") == -1, "Erase directory 'dir1' (can't because it isn't empty)");
	assertTest(eraseDir(fs, "dir2") == 0, "Erase directory 'dir2'");

	// List root directory again
	printf("Listing root directory after erasures:\n");
	listDir(fs);

	printf("Total tests ran:\t%d\n", testsRun);
	printf("Total tests passed:\t%d\n", testsPassed);

	free(memory);
}

int main()
{
	printf("Starting testing...\n");
	// Start timing tests
	clock_t beginningTime = clock();
	testFileSystem();

	// End timing tests
	clock_t endingTime = clock();
	printf("Ended testing.");
	printf("\nTime:\t%.3lfms\n", (double)(endingTime - beginningTime) / CLOCKS_PER_SEC * 1000);
	return 0;
}
