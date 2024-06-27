#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "file_system.h"

typedef int bool;
enum
{
	false,
	true
};

FileSystem *initializeFileSystem()
{
	FileSystem *fs = (FileSystem *)malloc(sizeof(FileSystem));
	fs->table = (int *)malloc(MAX_BLOCKS * sizeof(int));
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
		return NULL;
	}

	// Check if file already exists in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, fileName) == 0 &&
			fs->entries[i].type == FILE_TYPE)
		{
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

int eraseFile(FileSystem *fs, DirectoryEntry *file)
{
	if (file->type != FILE_TYPE)
	{
		return -1; // Not a file
	}

	int currentBlock = file->startBlock;
	while (currentBlock != -1)
	{
		int nextBlock = fs->table[currentBlock];
		fs->table[currentBlock] = -1; // Mark block as free
		currentBlock = nextBlock;
	}

	// Remove file from filesystem entries
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (&(fs->entries[i]) == file)
		{
			fs->entries[i] = fs->entries[--fs->entryCount];
			break;
		}
	}

	return 0;
}

FileHandle * open(FileSystem *fs, char *fileName) {
    // Search for the file in the current directory
    for (int i = 0; i < fs->entryCount; i++) {
        if (fs->entries[i].parentIndex == fs->currentDirIndex &&
            strcmp(fs->entries[i].name, fileName) == 0 &&
            fs->entries[i].type == FILE_TYPE) {
            
            FileHandle *fh = (FileHandle *)malloc(sizeof(FileHandle));
            fh->file = &(fs->entries[i]);
            fh->currentBlock = fh->file->startBlock;
            fh->currentPosition = 0;
            return fh;
        }
    }

    return NULL;  // File not found
}

void close(FileHandle *fh) {
    free(fh);
}

int write(FileSystem *fs, FileHandle *fh, char *data, int dataLength)
{
	if (dataLength <= 0)
		return -1;

	DirectoryEntry *file = fh->file;
	if (file->type != FILE_TYPE)
	{
		return -1; // Not a file
	}

	int blocksNeeded = (dataLength + BLOCK_SIZE - 1) / BLOCK_SIZE;
	int lastBlock = -1;
	int currentBlock = file->startBlock;

	// Find the last block of the file
	while (currentBlock != -1)
	{
		lastBlock = currentBlock;
		currentBlock = fs->table[currentBlock];
	}

	for (int i = 0; i < blocksNeeded; i++)
	{
		// Find a free block
		int freeBlock = -1;
		for (int j = 0; j < MAX_BLOCKS; j++)
		{
			if (fs->table[j] == -1)
			{
				freeBlock = j;
				break;
			}
		}

		if (freeBlock == -1)
		{
			return -1; // No free blocks
		}

		// Link the new block to the file
		if (lastBlock != -1)
		{
			fs->table[lastBlock] = freeBlock;
		}
		else
		{
			file->startBlock = freeBlock;
		}

		lastBlock = freeBlock;
		fs->table[freeBlock] = -1; // Mark as end of the chain

		// Copy data to the block
		int copyLength = (dataLength > BLOCK_SIZE) ? BLOCK_SIZE : dataLength;
		memcpy(fs->data[freeBlock], data, copyLength);
		data += copyLength;
		dataLength -= copyLength;
		file->size += copyLength;
	}

	return 0;
}

char *read(FileSystem *fs, FileHandle *fh, int dataLength)
{
	if (dataLength <= 0)
		return NULL;

	DirectoryEntry *file = fh->file;
	if (file->type != FILE_TYPE)
	{
		return NULL; // Not a file
	}

	int remainingData = file->size - fh->currentPosition;
	if (dataLength > remainingData)
	{
		dataLength = remainingData;
	}

	char *buffer = (char *)malloc(dataLength);
	char *bufferPtr = buffer;
	int readLength = dataLength;

	int currentBlock = fh->currentBlock;
	int blockOffset = fh->currentPosition % BLOCK_SIZE;

	while (readLength > 0 && currentBlock != -1)
	{
		int toRead = BLOCK_SIZE - blockOffset;
		if (toRead > readLength)
		{
			toRead = readLength;
		}

		memcpy(bufferPtr, fs->data[currentBlock] + blockOffset, toRead);
		bufferPtr += toRead;
		readLength -= toRead;
		fh->currentPosition += toRead;

		if (blockOffset + toRead >= BLOCK_SIZE)
		{
			currentBlock = fs->table[currentBlock];
			blockOffset = 0;
		}
	}

	return buffer;
}

int seek(FileSystem *fs, FileHandle *fh, int offset, int whence)
{
	int newPos;
	DirectoryEntry *file = fh->file;

	if (file->type != FILE_TYPE)
	{
		return -1;	// Not a file
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
		return -1;
	}

	if (newPos < 0 || newPos > file->size)
	{
		return -1;	// Invalid position
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

DirectoryEntry *createDir(FileSystem *fs, char *dirName)
{
	if (fs->entryCount >= MAX_ENTRIES)
	{
		return NULL; // Too many directories
	}

	// Check if directory already exists in the current directory
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == fs->currentDirIndex &&
			strcmp(fs->entries[i].name, dirName) == 0 &&
			fs->entries[i].type == DIRECTORY_TYPE)
		{
			return NULL; // Directory already exists
		}
	}

	DirectoryEntry *dir = &(fs->entries[fs->entryCount++]);
	strncpy(dir->name, dirName, MAX_FILENAME_LENGTH);
	dir->type = DIRECTORY_TYPE;
	dir->startBlock = -1; // No blocks allocated yet
	dir->size = 0;
	dir->parentIndex = fs->currentDirIndex;

	return dir;
}

int eraseDir(FileSystem *fs, DirectoryEntry *dir)
{
	if (dir->type != DIRECTORY_TYPE)
	{
		return -1; // Not a directory
	}

	// Check if directory is empty
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (fs->entries[i].parentIndex == (dir - fs->entries))
		{
			return -2; // Directory is not empty
		}
	}

	// Remove directory from filesystem entries
	for (int i = 0; i < fs->entryCount; i++)
	{
		if (&(fs->entries[i]) == dir)
		{
			fs->entries[i] = fs->entries[--fs->entryCount];
			break;
		}
	}

	return 0;
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