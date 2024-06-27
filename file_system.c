#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "file_system.h"

#define FREE_BLOCK -1
#define END_OF_CHAIN -2

struct DirectoryEntry
{
	char name[MAX_FILENAME_LENGTH];
	EntryType type;
	int startBlock;
	int size;
	int parentIndex; // Index of the parent directory
	time_t creationTimestamp;
	time_t lastAccessTimestamp;
};

struct FileSystem
{
	int *table;					// FAT table
	DirectoryEntry *entries;	// Directory entries
	char (*data)[BLOCK_SIZE];	// Data blocks as a 2D array
	int entryCount;				// How many entries currently present
	int maxEntries;				// How many entries at most
	int totalBlocks;			// How many blocks at most
	int currentDirIndex;		// Index of the current directory
};

struct FileHandle
{
	int fileIndex; // Index of the file in the entries array
	int currentBlock;
	int currentPosition;
};

FileSystem *initializeFileSystem(void *memory, size_t size)
{
	if (size < sizeof(FileSystem) + BLOCK_SIZE)
	{
		printf("Size: %ld, needed: %ld", size, sizeof(FileSystem) + BLOCK_SIZE);
		errno = ENOMEM;
		return NULL;
	}
	
	FileSystem *fs = (FileSystem *)memory;

	fs->totalBlocks = (size - sizeof(FileSystem)) / (BLOCK_SIZE + sizeof(int));
	fs->maxEntries = fs->totalBlocks; // Assuming one directory entry per block for simplicity

	// Allocate the FAT table, directory entries, and data blocks within the provided memory
	fs->table = (int *)((char *)memory + sizeof(FileSystem));
	fs->entries = (DirectoryEntry *)((char *)fs->table + sizeof(int) * fs->maxEntries);
	fs->data = (char(*)[BLOCK_SIZE])((char *)fs->entries + sizeof(fs->entries));

	for (int i = 0; i < fs->totalBlocks; i++)
	{
		fs->table[i] = FREE_BLOCK;
	}
	fs->entryCount = 0;

	// Create root directory
	DirectoryEntry *root = &(fs->entries[fs->entryCount++]);
	strncpy(root->name, "/", MAX_FILENAME_LENGTH);
	root->type = DIRECTORY_TYPE;
	root->startBlock = FREE_BLOCK;
	root->size = 0;
	root->parentIndex = -1; // Root has no parent

	fs->currentDirIndex = 0; // Set current directory to root

	return fs;
}

int createFile(FileSystem *fs, char *fileName)
{
	if (fs->entryCount >= fs->maxEntries)
	{
		errno = ENOSPC;
		return -1;
	}

	// Check if file already exists in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, fileName) == 0 &&
			fs->entries[i].type == FILE_TYPE)
		{
			errno = EEXIST;
			return -1; // File already exists
		}
	}

	DirectoryEntry *file = &(fs->entries[fs->entryCount++]);
	strncpy(file->name, fileName, MAX_FILENAME_LENGTH);

	file->type = FILE_TYPE;
	file->startBlock = FREE_BLOCK; // No blocks allocated yet
	file->size = 0;
	file->parentIndex = fs->currentDirIndex;

	return 0;
}

int eraseFile(FileSystem *fs, char *fileName)
{
	// Find the file to erase in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, fileName) == 0 &&
			fs->entries[i].type == FILE_TYPE)
		{

			// Erase the file
			fs->entries[i].type = FREE_BLOCK; // Mark as unused
			fs->entryCount--;
			return 0; // File erased successfully
		}
	}

	errno = ENOENT;
	return -1; // File not found
}

FileHandle *open(FileSystem *fs, char *fileName)
{
	FileHandle *fh = malloc(sizeof(FileHandle));
	if (fh == NULL)
	{
		return NULL; // Failed to allocate memory
	}

	fh->fileIndex = -1; // Initialize to invalid index

	// Find the file in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, fileName) == 0 &&
			fs->entries[i].type == FILE_TYPE)
		{

			// Found the file, store its index in the FileHandle
			fh->fileIndex = i;
			fh->currentBlock = fs->entries[i].startBlock;
			fh->currentPosition = 0;
			fs->entries[i].creationTimestamp = time(NULL);
			fs->entries[i].lastAccessTimestamp = fs->entries[i].creationTimestamp;
			return fh; // Return the FileHandle pointer
		}
	}

	free(fh); // Clean up on failure
	errno = ENOENT;
	return NULL; // File not found
}

void close(FileHandle *fh)
{
	free(fh);
}

int write(FileSystem *fs, FileHandle *fh, char *data, int dataLength)
{
	int bytesWritten = 0;
	DirectoryEntry *file = &fs->entries[fh->fileIndex];
	file->lastAccessTimestamp = time(NULL);

	while (bytesWritten < dataLength)
	{
		if (fh->currentBlock == FREE_BLOCK)
		{
			// Allocate a new block if needed
			int freeBlock = -1;
			for (int i = 0; i < fs->totalBlocks; i++)
			{
				if (fs->table[i] == FREE_BLOCK)
				{
					freeBlock = i;
					fs->table[i] = END_OF_CHAIN;
					if (file->startBlock == FREE_BLOCK)
					{
						file->startBlock = i;
					}
					else
					{
						int lastBlock = file->startBlock;
						while (fs->table[lastBlock] != -END_OF_CHAIN)
						{
							lastBlock = fs->table[lastBlock];
						}
						fs->table[lastBlock] = i;
					}
					fh->currentBlock = i;
					break;
				}
			}

			if (freeBlock == -1)
			{
				errno = ENOSPC;
				return -1; // No free blocks available
			}
		}

		int blockOffset = fh->currentPosition % BLOCK_SIZE;
		int bytesToWrite = BLOCK_SIZE - blockOffset;

		if (bytesToWrite > dataLength - bytesWritten)
		{
			bytesToWrite = dataLength - bytesWritten;
		}

		memcpy(fs->data[fh->currentBlock] + blockOffset, data + bytesWritten, bytesToWrite);

		fh->currentPosition += bytesToWrite;
		bytesWritten += bytesToWrite;

		if (fh->currentPosition % BLOCK_SIZE == 0)
		{
			int nextBlock = fs->table[fh->currentBlock];
			if (nextBlock == END_OF_CHAIN)
			{
				nextBlock = FREE_BLOCK;
			}
			fh->currentBlock = nextBlock;
		}
	}

	file->size += bytesWritten;
	return bytesWritten;
}

char *read(FileSystem *fs, FileHandle *fh, int dataLength)
{
	char *buffer = (char *)malloc(dataLength + 1);
	int bytesRead = 0;

	fs->entries[fh->fileIndex].lastAccessTimestamp = time(NULL);

	while (bytesRead < dataLength && fh->currentBlock != FREE_BLOCK)
	{
		int blockOffset = fh->currentPosition % BLOCK_SIZE;
		int bytesToRead = BLOCK_SIZE - blockOffset;

		if (bytesToRead > dataLength - bytesRead)
		{
			bytesToRead = dataLength - bytesRead;
		}

		memcpy(buffer + bytesRead, fs->data[fh->currentBlock] + blockOffset, bytesToRead);

		fh->currentPosition += bytesToRead;
		bytesRead += bytesToRead;

		if (fh->currentPosition % BLOCK_SIZE == 0)
		{
			fh->currentBlock = fs->table[fh->currentBlock];
		}
	}

	buffer[bytesRead] = '\0'; // Null-terminate the string
	return buffer;
}

int seek(FileSystem *fs, FileHandle *fh, int offset, int whence)
{
	int newPos;
	DirectoryEntry *file = &fs->entries[fh->fileIndex];
	file->lastAccessTimestamp = time(NULL);

	if (file->type != FILE_TYPE)
	{
		errno = EISDIR;
		return -1; // Not a file
	}

	switch (whence)
	{
	case SEEK_BEGIN:
		newPos = offset;
		break;
	case SEEK_CURRENT:
		newPos = fh->currentPosition + offset;
		break;
	case SEEK_ENDING:
		newPos = file->size - offset;
		break;
	default:
		errno = EINVAL;
		return -1; // Invalid "whence"
	}

	if (newPos < 0 || newPos > file->size)
	{
		errno = EINVAL;
		return -1; // Invalid position
	}

	fh->currentPosition = newPos;
	fh->currentBlock = file->startBlock;

	int blocksToSeek = newPos / BLOCK_SIZE;
	for (int i = 0; i < blocksToSeek; i++)
	{
		fh->currentBlock = fs->table[fh->currentBlock];
	}

	return 0;
}

int createDir(FileSystem *fs, char *dirName)
{
	if (fs->entryCount >= fs->maxEntries)
	{
		errno = ENOSPC;
		return -1; // Too many directories
	}

	// Check if directory already exists in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, dirName) == 0 &&
			fs->entries[i].type == DIRECTORY_TYPE)
		{
			errno = EEXIST;
			return -1; // Directory already exists
		}
	}

	DirectoryEntry *dir = &(fs->entries[fs->entryCount++]);
	strncpy(dir->name, dirName, MAX_FILENAME_LENGTH);
	dir->type = DIRECTORY_TYPE;
	dir->startBlock = FREE_BLOCK; // No blocks allocated yet
	dir->size = 0;
	dir->parentIndex = fs->currentDirIndex;
	dir->creationTimestamp = time(NULL);
	dir->lastAccessTimestamp = dir->creationTimestamp;

	return 0;
}

int eraseDir(FileSystem *fs, char *dirName)
{
	// Find the directory to erase in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, dirName) == 0 &&
			fs->entries[i].type == DIRECTORY_TYPE)
		{

			// Erase the directory and its contents
			int dirIndex = i;
			for (int j = 0; j < fs->entryCount; j++)
			{
				if (fs->entries[j].parentIndex == dirIndex)
				{
					// Erase files and subdirectories recursively
					if (fs->entries[j].type == FILE_TYPE)
					{
						eraseFile(fs, fs->entries[j].name);
					}
					else if (fs->entries[j].type == DIRECTORY_TYPE)
					{
						eraseDir(fs, fs->entries[j].name);
					}
				}
			}

			// Remove the directory entry itself
			fs->entries[dirIndex].type = FREE_BLOCK; // Mark as unused
			fs->entryCount--;
			return 0; // Directory erased successfully
		}
	}

	errno = ENOENT;
	return -1; // Directory not found
}

int changeDir(FileSystem *fs, char *dirName)
{
	// Handle special case for root directory
	if (strcmp(dirName, "/") == 0)
	{
		fs->currentDirIndex = 0;
		return 0;
	}

	// Search for the directory in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, dirName) == 0 &&
			fs->entries[i].type == DIRECTORY_TYPE)
		{
			fs->currentDirIndex = i;
			fs->entries[i].lastAccessTimestamp = time(NULL);
			return 0;
		}
	}

	errno = ENOENT;
	return -1; // Directory not found
}

void listDir(FileSystem *fs)
{
	fs->entries[fs->currentDirIndex].lastAccessTimestamp = time(NULL);
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex)
		{
			if (fs->entries[i].type == DIRECTORY_TYPE)
			{
				printf("DIR  %s\n", fs->entries[i].name);
			}
			else if (fs->entries[i].type == FILE_TYPE)
			{
				printf("FILE %s\n", fs->entries[i].name);
			}
		}
	}
}