#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "file_system.h"

typedef int bool;
enum
{
	false,
	true
};

FileSystem *initializeFileSystem(void *memory, size_t size)
{
	// Calculate the required sizes for different components
	size_t fatSize = MAX_BLOCKS * sizeof(int);
	size_t entriesSize = MAX_ENTRIES * sizeof(DirectoryEntry);
	size_t dataSize = MAX_BLOCKS * BLOCK_SIZE;

	// Ensure the provided memory is large enough
	if (size < fatSize + entriesSize + dataSize)
	{
		errno = ENOMEM;
		return NULL;
	}

	FileSystem *fs = (FileSystem *)memory;

	// Allocate the FAT table, directory entries, and data blocks within the provided memory
	fs->table = (int *)((char *)memory + sizeof(FileSystem));
	fs->entries = (DirectoryEntry *)((char *)fs->table + fatSize);
	fs->data = (char(*)[BLOCK_SIZE])((char *)fs->entries + entriesSize);

	for (int i = 0; i < MAX_BLOCKS; i++)
	{
		fs->table[i] = -1; // -1 indicates free block
	}
	fs->entryCount = 0;

	// Create root directory
	DirectoryEntry *root = &(fs->entries[fs->entryCount++]);
	strncpy(root->name, "/", MAX_FILENAME_LENGTH);
	root->type = DIRECTORY_TYPE;
	root->startBlock = -1;
	root->size = 0;
	root->parentIndex = -1; // Root has no parent

	fs->currentDirIndex = 0; // Set current directory to root

	return fs;
}

DirectoryEntry *createFile(FileSystem *fs, char *fileName)
{
	if (fs->entryCount >= MAX_ENTRIES)
	{
		errno = ENOSPC;
		return NULL;
	}

	// Check if file already exists in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, fileName) == 0 &&
			fs->entries[i].type == FILE_TYPE)
		{
			errno = EEXIST;
			return NULL; // File already exists
		}
	}

	DirectoryEntry *file = &(fs->entries[fs->entryCount++]);
	strncpy(file->name, fileName, MAX_FILENAME_LENGTH);
	file->type = FILE_TYPE;
	file->startBlock = -1; // No blocks allocated yet
	file->size = 0;
	file->parentIndex = fs->currentDirIndex;

	return file;
}

int eraseFile(FileSystem *fs, char* fileName) {
    // Find the file to erase in the current directory
    for (int i = 0; i < fs->entryCount; i++) {
        if (fs->entries[i].parentIndex == fs->currentDirIndex &&
            strcmp(fs->entries[i].name, fileName) == 0 &&
            fs->entries[i].type == FILE_TYPE) {
            
            // Erase the file
            fs->entries[i].type = -1;  // Mark as unused
            fs->entryCount--;
            return 0;  // File erased successfully
        }
    }

	errno = ENOENT;
    return -1;  // File not found
}

FileHandle *open(FileSystem *fs, char *fileName)
{
	// Search for the file in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, fileName) == 0 &&
			fs->entries[i].type == FILE_TYPE)
		{

			FileHandle *fh = (FileHandle *)malloc(sizeof(FileHandle));
			fh->file = &(fs->entries[i]);
			fh->currentBlock = fh->file->startBlock;
			fh->currentPosition = 0;
			return fh;
		}
	}
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

	while (bytesWritten < dataLength)
	{
		if (fh->currentBlock == -1)
		{
			// Allocate a new block if needed
			int freeBlock = -1;
			for (int i = 0; i < MAX_BLOCKS; i++)
			{
				if (fs->table[i] == -1)
				{
					freeBlock = i;
					fs->table[i] = -2; // -2 indicates end of chain
					if (fh->file->startBlock == -1)
					{
						fh->file->startBlock = i;
					}
					else
					{
						int lastBlock = fh->file->startBlock;
						while (fs->table[lastBlock] != -2)
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
			if (nextBlock == -2)
			{
				nextBlock = -1;
			}
			fh->currentBlock = nextBlock;
		}
	}

	fh->file->size += bytesWritten;
	return bytesWritten;
}

char *read(FileSystem *fs, FileHandle *fh, int dataLength)
{
	char *buffer = (char *)malloc(dataLength + 1);
	int bytesRead = 0;

	while (bytesRead < dataLength && fh->currentBlock != -1)
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
	DirectoryEntry *file = fh->file;

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
		newPos = file->size + offset;
		break;
	default:
		errno = EINVAL;
		return -1;
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
	if (fs->entryCount >= MAX_ENTRIES)
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
	dir->startBlock = -1; // No blocks allocated yet
	dir->size = 0;
	dir->parentIndex = fs->currentDirIndex;

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
			fs->entries[dirIndex].type = -1; // Mark as unused
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
			return 0;
		}
	}

	errno = ENOENT;
	return -1; // Directory not found
}

void listDir(FileSystem *fs)
{
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