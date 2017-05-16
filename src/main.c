//----------------------------------------------------------------------------------------------------------------------
// PE File Format examiner
//----------------------------------------------------------------------------------------------------------------------

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//----------------------------------------------------------------------------------------------------------------------
// Type definitions

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

#define internal static

//----------------------------------------------------------------------------------------------------------------------
// Memory mapped files

typedef struct _File
{
    const void*     buffer;
    i64             size;

    HANDLE          file;
    HANDLE          fileMap;
}
File;

internal File memoryMapFile(const char* fileName)
{
    File f;

    f.file = CreateFileA(fileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (f.file)
    {
        DWORD fileSizeHigh, fileSizeLow;

        fileSizeLow = GetFileSize(f.file, &fileSizeHigh);
        f.fileMap = CreateFileMappingA(f.file, 0, PAGE_READONLY, fileSizeHigh, fileSizeLow, 0);

        if (f.fileMap)
        {
            f.buffer = MapViewOfFile(f.fileMap, FILE_MAP_READ, 0, 0, 0);
            f.size = ((i64)fileSizeHigh << 32) | fileSizeLow;
        }
    }

    return f;
}

internal void memoryUnmapFile(File* f)
{
    if (f->buffer)      UnmapViewOfFile(f->buffer);
    if (f->fileMap)     CloseHandle(f->fileMap);
    if (f->file)        CloseHandle(f->file);
}

//----------------------------------------------------------------------------------------------------------------------
// Output

#define LINE_LENGTH 80

internal void title(const char* title)
{
    size_t titleLen = strlen(title);
    size_t tailLineLen = LINE_LENGTH - titleLen - 4;

    assert(titleLen < (LINE_LENGTH - 4));
    fprintf(stdout, "-- %s ", title);

    for (int i = 0; i < tailLineLen; ++i) fprintf(stdout, "-");

    fprintf(stdout, "\n\n");
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry

int main(int argc, char** argv)
{
    File f = memoryMapFile(argv[0]);
    memoryUnmapFile(&f);

    title("This is a test");
    title("So is this!");

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
