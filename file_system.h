#define BLOCK_SIZE 512
#define MAX_FILENAME_LENGTH 64

enum
{
	SEEK_BEGIN,
	SEEK_CURRENT,
	SEEK_ENDING
};

typedef struct FileSystem FileSystem;
typedef struct DirectoryEntry DirectoryEntry;
typedef struct FileHandle FileHandle;

typedef struct {
	int size;
	time_t creationTimestamp;
	time_t lastAccessTimestamp;
} attributes;

// Filesystem functions
FileSystem *initializeFileSystem();
size_t getTotalSize(FileSystem* fs);
size_t getOccupiedSize(FileSystem* fs);

// File functions
int createFile(FileSystem *fs, char *fileName);
int eraseFile(FileSystem *fs, char *fileName);
FileHandle * open(FileSystem *fs, char *fileName);
void close(FileHandle *fh);
int write(FileSystem *fs, FileHandle *fh, char *data, int dataLength);
char *read(FileSystem *fs, FileHandle *fh, int dataLength);
int seek(FileSystem *fs, FileHandle *fh, int offset, int whence);
int getAttributes(FileSystem *fs, FileHandle *fh, attributes *attr);

// Directory functions
int createDir(FileSystem *fs, char *dirName);
int eraseDir(FileSystem *fs, char *dirName);
int changeDir(FileSystem *fs, char *dirName);
void listDir(FileSystem *fs);
