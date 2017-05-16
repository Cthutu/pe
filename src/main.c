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
    const u8*       buffer;
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
#define P(...) fprintf(stdout, __VA_ARGS__)

internal void title(const char* title)
{
    size_t titleLen = strlen(title);
    size_t tailLineLen = LINE_LENGTH - titleLen - 4;

    assert(titleLen < (LINE_LENGTH - 4));
    P("-- %s ", title);

    for (int i = 0; i < tailLineLen; ++i) P("-");

    P("\n\n");
}

#define PN() P("\n")
#define P_I16(lead, value) P("%s: %04hx (%d)\n", lead, value, value)
#define P_I32(lead, value) P("%s: %08x (%d)\n", lead, value, value)
#define P_A16(lead, value) P("%s: %04hx\n", lead, value)

//----------------------------------------------------------------------------------------------------------------------
// DOS Header

typedef struct _DosHeader
{
    char        signature[2];
    i16         lastSize;
    i16         numBlocks;
    i16         numReloc;
    i16         hdrSize;
    i16         minAlloc;
    i16         maxAlloc;
    u16         ss;
    u16         sp;
    i16         checkSum;
    u16         ip;
    u16         cs;
    i16         relocPos;
    i16         numOverlays;
    i16         reserved1[4];
    i16         oemId;
    i16         oemInfo;
    i16         reserved2[10];
    i32         e_lfanew;
}
DosHeader;

internal i32 dosHeader(const u8* start)
{
    DosHeader* hdr = (DosHeader *)start;

    title("DOS Header");

    P("  signature: '%c%c'\n", hdr->signature[0], hdr->signature[1]);
    P_I16("   lastSize", hdr->lastSize);
    P_I16("  numBlocks", hdr->numBlocks);
    P_I16("   numReloc", hdr->numReloc);
    P_I16("    hdrSize", hdr->hdrSize);
    P_I16("   minAlloc", hdr->minAlloc);
    P_I16("   maxAlloc", hdr->maxAlloc);
    P_A16("         SS", hdr->ss);
    P_A16("         SP", hdr->sp);
    P_I16("   checksum", hdr->checkSum);
    P_A16("         IP", hdr->ip);
    P_A16("         CS", hdr->cs);
    P_I16("   relocPos", hdr->relocPos);
    P_I16("numOverlays", hdr->numOverlays);
    P_I16("     OEM Id", hdr->oemId);
    P_I16("   OEM Info", hdr->oemInfo);
    P_I32("    LFA New", hdr->e_lfanew);

    PN();

    return hdr->e_lfanew;
}

//----------------------------------------------------------------------------------------------------------------------
// COFF Header

typedef struct _CoffHeader
{
    i16     machine;
    i16     numberOfSections;
    i32     timeDateStamp;
    i32     pointerToSymbolTable;
    i32     numberOfSymbols;
    i16     sizeOfOptionHeader;
    i16     characteristics;
}
CoffHeader;

internal const char* getMachine(i16 code)
{
    struct { i16 code; const char* name; } machines[] = {
        { 0x014c, "Intel 386" },
        { 0x8664, "x64 / AMD AMD64" },
        { 0x0162, "MIPS R3000" },
        { 0x0168, "MIPS R10000" },
        { 0x0169, "MIPS little endian WCI v2" },
        { 0x0183, "Old Alpha AXP" },
        { 0x0184, "Alpha AXP" },
        { 0x01a2, "Hitachi SH3" },
        { 0x01a3, "Hitachi SH3 DSP" },
        { 0x01a6, "Hitachi SH4" },
        { 0x01a8, "Hitachi SH5" },
        { 0x01c0, "ARM little endian" },
        { 0x01c2, "Thumb" },
        { 0x01d3, "Matsushita AM33" },
        { 0x01f0, "PowerPC little endian" },
        { 0x01f1, "PowerPC with floating point support" },
        { 0x0200, "Intel IA64" },
        { 0x0266, "MIPS16" },
        { 0x0268, "Motorola 68000 series" },
        { 0x0284, "Alpha AXP 64-bit" },
        { 0x0366, "MIPS with FPU" },
        { 0x0466, "MIPS16 with FPU" },
        { 0x0ebc, "EFI Byte Code" },
        { 0x9041, "Mitsubishi M32R little endian" },
        { 0xc0ee, "CLR pure MSIL" },
    };

    for (int i = 0; i < sizeof(machines) / sizeof(machines[0]); ++i)
    {
        if (machines[i].code == code) return machines[i].name;
    }

    return "Unknown";
}

internal coffHeader(const u8* start)
{
    CoffHeader* hdr = (CoffHeader *)start;
    title("COFF Header");

    P("             Machine: %s\n", getMachine(hdr->machine));
    P_I16("  Number of sections", hdr->numberOfSections);
    P_I32("     Time/Date stamp", hdr->timeDateStamp);
    P_I32("   Number of symbols", hdr->numberOfSymbols);
    P_I16("Optional header size", hdr->sizeOfOptionHeader);
    P("     Characteristics: ");
    if (hdr->characteristics & 0x02) P("EXECUTABLE ");
    if (hdr->characteristics & 0x200) P("NON-RELOCATABLE");
    if (hdr->characteristics & 0x2000) P("DLL");
    P("\n");
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry

int main(int argc, char** argv)
{
    File f = memoryMapFile(argv[0]);
    i16 coffHeaderRVA = dosHeader(f.buffer);
    coffHeader(f.buffer + coffHeaderRVA + 4);
    memoryUnmapFile(&f);

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
