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
#define P_I16(lead, value) P("%s: 0x%04hx (%d)\n", lead, value, value)
#define P_I32(lead, value) P("%s: 0x%08x (%d)\n", lead, value, value)
#define P_I64(lead, value) P("%s: 0x%016llx (%lld)\n", lead, value, value)
#define P_A16(lead, value) P("%s: 0x%04hx\n", lead, value)
#define P_A32(lead, value) P("%s: 0x%08x\n", lead, value)
#define P_A64(lead, value) P("%s: 0x%016llx\n", lead, value)

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

internal i64 coffHeader(const u8* start, int* is64)
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

    PN();

    *is64 = (hdr->machine == (i16)0x8664) ? 1 : 0;

    return (i64)sizeof(CoffHeader);
}

//----------------------------------------------------------------------------------------------------------------------
// PE Optional Header

typedef struct _DataDirectory
{
    i32     virtualAddress;
    i32     size;
}
DataDirectory;

typedef struct _OptionalHeader
{
    i16             signature;
    i8              majorLinkerVersion;
    i8              minorLinkerVersion;
    i32             sizeOfCode;
    i32             sizeOfInitialisedData;
    i32             sizeOfUninitalisedData;
    i32             addressOfEntryPoint;
    i32             baseOfCode;
    i32             baseOfData;

    i32             imageBase;
    i32             sectionAlignment;
    i32             fileAlignment;
    i16             majorOSVersion;
    i16             minorOSVersion;
    i16             majorImageVersion;
    i16             minorImageVersion;
    i16             majorSubsystemVersion;
    i16             minorSubsystemVersion;
    i32             win32VersionValue;
    i32             sizeImage;
    i32             sizeHeaders;
    i32             checkSum;
    i16             subSytem;
    i16             dllCharacteristics;
    i32             sizeOfStackReserve;
    i32             sizeOfStackCommit;
    i32             sizeOfHeapReserve;
    i32             sizeOfHeapCommit;
    i32             loaderFlags;
    i32             numberOfRvaAndSizes;
    DataDirectory   dataDirectory[0];
}
OptionalHeader;

typedef struct _OptionalHeader64
{
    i16             signature;
    i8              majorLinkerVersion;
    i8              minorLinkerVersion;
    i32             sizeOfCode;
    i32             sizeOfInitialisedData;
    i32             sizeOfUninitalisedData;
    i32             addressOfEntryPoint;
    i32             baseOfCode;

    i64             imageBase;
    i32             sectionAlignment;
    i32             fileAlignment;
    i16             majorOSVersion;
    i16             minorOSVersion;
    i16             majorImageVersion;
    i16             minorImageVersion;
    i16             majorSubsystemVersion;
    i16             minorSubsystemVersion;
    i32             win32VersionValue;
    i32             sizeImage;
    i32             sizeHeaders;
    i32             checkSum;
    i16             subSytem;
    i16             dllCharacteristics;
    i64             sizeOfStackReserve;
    i64             sizeOfStackCommit;
    i64             sizeOfHeapReserve;
    i64             sizeOfHeapCommit;
    i32             loaderFlags;
    i32             numberOfRvaAndSizes;
    DataDirectory   dataDirectory[0];
}
OptionalHeader64;

internal i64 peHeader32(const u8* start)
{
    const OptionalHeader* hdr = (const OptionalHeader *)start;
    int subSystem;

    title("Optional PE Header");

    P("                 Signature: ");
    switch (hdr->signature)
    {
    case 0x10b:     P("32-bit executable image\n");         break;
    case 0x20b:     P("64-bit executable image\n");         break;
    case 0x107:     P("ROM image\n");                       break;
    default:        P("Unknown\n");                         break;
    }

    P("            Linker version: %d.%d\n", (int)hdr->majorImageVersion, (int)hdr->minorLinkerVersion);
    P_I32("              Size of code", hdr->sizeOfCode);
    P_I32("  Size of initialised data", hdr->sizeOfInitialisedData);
    P_I32("Size of uninitialised data", hdr->sizeOfUninitalisedData);
    P_I32("    Address of entry point", hdr->addressOfEntryPoint);
    P_I32("              Base of code", hdr->baseOfCode);
    P_I32("              Base of data", hdr->baseOfData);
    P_A32("                Image base", hdr->imageBase);
    P_I32("         Section alignment", hdr->sectionAlignment);
    P_I32("            File alignment", hdr->fileAlignment);
    P("                OS version: %d.%d\n", (int)hdr->majorOSVersion, (int)hdr->minorOSVersion);
    P("             Image version: %d.%d\n", (int)hdr->majorImageVersion, (int)hdr->minorImageVersion);
    P("         Subsystem version: %d.%d\n", (int)hdr->majorSubsystemVersion, (int)hdr->minorSubsystemVersion);
    P_I32("            Size of header", hdr->sizeHeaders);
    P_I32("                  Checksum", hdr->checkSum);

    const char* subSystems[] = {
        "Unknown subsystem",
        "No subsystem required",
        "Windows GUI",
        "Windows character mode",
        "Unknown subsystem",
        "OS/2 CUI",
        "Unknown subsystem",
        "POSIX CUI",
        "Unknown subsystem",
        "Windows CE",
        "EFI application",
        "EFI driver with boot services",
        "EFI driver with run-time services",
        "EFI ROM image",
        "Xbox",
        "Unknown subsystem",
        "Boot application",
    };
    subSystem = hdr->subSytem;
    if (subSystem < 0 || subSystem > 16) subSystem = 0;
    P("                 Subsystem: %s\n", subSystems[subSystem]);

    if (hdr->dllCharacteristics)
    {
        P("       DLL characteristics: ");
        if (hdr->dllCharacteristics & 0x0040) P("RELOCATABLE ");
        if (hdr->dllCharacteristics & 0x0080) P("INTEGRITY-FORCED ");
        if (hdr->dllCharacteristics & 0x0100) P("DEP-COMPATIBLE ");
        if (hdr->dllCharacteristics & 0x0200) P("NO-ISOLATION ");
        if (hdr->dllCharacteristics & 0x0400) P("NO-SEH ");
        if (hdr->dllCharacteristics & 0x0800) P("NO-BIND ");
        if (hdr->dllCharacteristics & 0x2000) P("WDM-DRIVER ");
        if (hdr->dllCharacteristics & 0x8000) P("TERMINAL-SERVER-AWARE ");
        P("(0x%04hx)", hdr->dllCharacteristics);
        PN();
    }
    else
    {
        P("       DLL characteristics: None (0x0000)\n");
    }

    P_I32("     Size of stack reserve", hdr->sizeOfStackReserve);
    P_I32("      Size of stack commit", hdr->sizeOfStackCommit);
    P_I32("      Size of heap reserve", hdr->sizeOfHeapReserve);
    P_I32("       Size of heap commit", hdr->sizeOfHeapCommit);
    P_I32("   Loader flags (obsolete)", hdr->loaderFlags);
    P_I32("  Number of RVAs and sizes", hdr->numberOfRvaAndSizes);
    


    PN();

    return 0;
}

internal i64 peHeader64(const u8* start)
{
    const OptionalHeader64* hdr = (const OptionalHeader64 *)start;
    int subSystem;

    title("Optional PE Header");

    P("                 Signature: ");
    switch (hdr->signature)
    {
    case 0x10b:     P("32-bit executable image\n");         break;
    case 0x20b:     P("64-bit executable image\n");         break;
    case 0x107:     P("ROM image\n");                       break;
    default:        P("Unknown\n");                         break;
    }

    P("            Linker version: %d.%d\n", (int)hdr->majorImageVersion, (int)hdr->minorLinkerVersion);
    P_I32("              Size of code", hdr->sizeOfCode);
    P_I32("  Size of initialised data", hdr->sizeOfInitialisedData);
    P_I32("Size of uninitialised data", hdr->sizeOfUninitalisedData);
    P_I32("    Address of entry point", hdr->addressOfEntryPoint);
    P_I32("              Base of code", hdr->baseOfCode);
    P_A64("                Image base", hdr->imageBase);
    P_I32("         Section alignment", hdr->sectionAlignment);
    P_I32("            File alignment", hdr->fileAlignment);
    P("                OS version: %d.%d\n", (int)hdr->majorOSVersion, (int)hdr->minorOSVersion);
    P("             Image version: %d.%d\n", (int)hdr->majorImageVersion, (int)hdr->minorImageVersion);
    P("         Subsystem version: %d.%d\n", (int)hdr->majorSubsystemVersion, (int)hdr->minorSubsystemVersion);
    P_I32("            Size of header", hdr->sizeHeaders);
    P_I32("                  Checksum", hdr->checkSum);

    const char* subSystems[] = {
        "Unknown subsystem",
        "No subsystem required",
        "Windows GUI",
        "Windows character mode",
        "Unknown subsystem",
        "OS/2 CUI",
        "Unknown subsystem",
        "POSIX CUI",
        "Unknown subsystem",
        "Windows CE",
        "EFI application",
        "EFI driver with boot services",
        "EFI driver with run-time services",
        "EFI ROM image",
        "Xbox",
        "Unknown subsystem",
        "Boot application",
    };
    subSystem = hdr->subSytem;
    if (subSystem < 0 || subSystem > 16) subSystem = 0;
    P("                 Subsystem: %s\n", subSystems[subSystem]);

    if (hdr->dllCharacteristics)
    {
        P("       DLL characteristics: ");
        if (hdr->dllCharacteristics & 0x0040) P("RELOCATABLE ");
        if (hdr->dllCharacteristics & 0x0080) P("INTEGRITY-FORCED ");
        if (hdr->dllCharacteristics & 0x0100) P("DEP-COMPATIBLE ");
        if (hdr->dllCharacteristics & 0x0200) P("NO-ISOLATION ");
        if (hdr->dllCharacteristics & 0x0400) P("NO-SEH ");
        if (hdr->dllCharacteristics & 0x0800) P("NO-BIND ");
        if (hdr->dllCharacteristics & 0x2000) P("WDM-DRIVER ");
        if (hdr->dllCharacteristics & 0x8000) P("TERMINAL-SERVER-AWARE ");
        P("(0x%04hx)", hdr->dllCharacteristics);
        PN();
    }
    else
    {
        P("       DLL characteristics: None (0x0000)\n");
    }

    P_I64("     Size of stack reserve", hdr->sizeOfStackReserve);
    P_I64("      Size of stack commit", hdr->sizeOfStackCommit);
    P_I64("      Size of heap reserve", hdr->sizeOfHeapReserve);
    P_I64("       Size of heap commit", hdr->sizeOfHeapCommit);
    P_I32("   Loader flags (obsolete)", hdr->loaderFlags);
    P_I32("  Number of RVAs and sizes", hdr->numberOfRvaAndSizes);



    PN();

    return 0;
}

internal i64 peHeader(const u8* start, int is64)
{
    if (is64)
    {
        return peHeader64(start);
    }
    else
    {
        return peHeader32(start);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry

int main(int argc, char** argv)
{
    int i = 0;
    int is64;

    if (argc > 1)
    {
        i = 1;
    }

    File f = memoryMapFile(argv[i]);

    i64 p = (i64)dosHeader(f.buffer);
    p += 4;
    p += coffHeader(f.buffer + p, &is64);
    p += peHeader(f.buffer + p, is64);

    memoryUnmapFile(&f);

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
