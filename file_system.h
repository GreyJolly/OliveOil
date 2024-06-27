#define MAX_BLOCKS 1024
#define MAX_ENTRIES 128
#define BLOCK_SIZE 512
#define MAX_FILENAME_LENGTH 64

enum {SEEK_BEGIN, SEEK_CURRENT, SEEK_ENDING};
typedef enum {FILE_TYPE, DIRECTORY_TYPE} EntryType;

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    EntryType type;
    int startBlock;
    int size;
    int parentIndex;  // Index of the parent directory
} DirectoryEntry;

typedef struct {
    int *table;  // File Allocation Table
    DirectoryEntry entries[MAX_ENTRIES];
    char data[MAX_BLOCKS][BLOCK_SIZE];  // Data blocks
    int entryCount;
    int currentDirIndex;  // Index of the current directory
} FileSystem;

typedef struct {
	DirectoryEntry * file;
	int currentBlock;
	int currentPosition;
} FileHandle;

FileSystem * initializeFileSystem();
DirectoryEntry *createFile(FileSystem *fs, char *fileName);
int eraseFile(FileSystem *fs, DirectoryEntry *file);
FileHandle * open(FileSystem *fs, char *fileName);
void close(FileHandle *fh);
int write(FileSystem *fs, FileHandle *fh, char *data, int dataLength);
char *read(FileSystem *fs, FileHandle *fh, int dataLength);
int seek(FileSystem *fs, FileHandle *fh, int offset, int whence);
DirectoryEntry *createDir(FileSystem *fs, char *dirName);
int eraseDir(FileSystem *fs, DirectoryEntry *dir);
int changeDir(FileSystem *fs, char *dirName);
void listDir(FileSystem *fs);