//
// Created by admin on 2024/3/6.
//

#ifndef DOTNETFILEPARSER_COMMON_H
#define DOTNETFILEPARSER_COMMON_H

#include <cstdint>

#define ALIGN_WITH(a, b)((a)%(b)?((a)+(b)-((a)%(b))):(a))

typedef struct MZHeader {
  uint16_t magicNumber; // 0x0
  uint16_t lastPageBytes; // 0x2
  uint16_t howManyPages; // 0x4
  uint16_t relocations; // 0x6
  uint16_t headerSize; // 0x8
  uint16_t minMemory; // 0xa
  uint16_t maxMemory; // 0xc
  uint16_t initStackSegment; // 0xe
  uint16_t initStackPointer; // 0x10
  uint16_t checksum; // 0x12
  uint16_t initInstructionPointer; // 0x14
  uint16_t initCodeSegment; // 0x16
  uint16_t relocationTableAddress; // 0x18
  uint16_t howManyOverlays; // 0x1a
  uint16_t reserved1[4]; // 0x1c
  uint16_t oemId; // 0x24
  uint16_t oemInfo; // 0x26
  uint16_t reserved2[10]; // 0x28
  uint32_t peHeaderPointer; // 0x3c
} MZHeader;

typedef struct COFFHeader {
  uint32_t signature; // 0x00
  uint16_t machineCode; // 0x04
  uint16_t numberOfSections;  // 0x06
  uint32_t timeStamp;
  uint32_t pointerToSymbolTable;
  uint32_t numberOfSymbolTable;
  uint16_t sizeOfOptionalHeader;
  uint16_t fileCharacteristics;
} COFFHeader;

typedef struct StandardCOFFHeader {
  uint16_t magicValue;
  uint8_t linkerMajorVersion;
  uint8_t linkerMinorVersion;
  uint32_t sizeOfCode;
  uint32_t sizeOfInitializedData;
  uint32_t sizeOfUninitializedData;
  uint32_t addressOfEntryPoint;
  uint32_t baseOfCode;
  uint32_t baseOfData;
} StandardCOFFHeader;

typedef struct WindowsSpecificHeader {
  uint32_t imageBase;
  uint32_t sectionAlignment;
  uint32_t fileAlignment;
  uint16_t osMajorVersion;
  uint16_t osMinorVersion;
  uint16_t imageMajorVersion;
  uint16_t imageMinorVersion;
  uint16_t subsystemMajorVersion;
  uint16_t subsystemMinorVersion;
  uint32_t win32VersionValue;
  uint32_t sizeOfImage;
  uint32_t sizeOfHeaders;
  uint32_t checksum;
  uint16_t subsystem;
  uint16_t dllCharacteristics;
  uint32_t sizeOfStackReserve;
  uint32_t sizeOfStackCommit;
  uint32_t sizeOfHeapReserve;
  uint32_t sizeOfHeapCommit;
  uint32_t loaderFlags;
  uint32_t numberOfRvaAndSizes;
} WindowsSpecificHeader;

typedef struct PEHeader {
  COFFHeader coffHeader;
  StandardCOFFHeader standardCoffHeader;
  WindowsSpecificHeader windowsSpecificHeader;
} PEHeader;


#define DataExport 0
#define DataImport 1
#define DataResource 2
#define DataException 3
#define DataSecurity 4
#define DataRelocation 5
#define DataDebug 6
#define DataCopyright 7
#define DataGlobalPtr 8
#define DataThreadLocalStorage 9
#define DataLoadConfig 10
#define DataBoundImport 11
#define DataImportAddressTable 12
#define DataDelayLoadImportAddressTable 13
#define DataCLRHeader 14
#define DataReserved 15


// RVA & size
typedef struct DataDirectory {
  uint32_t address;
  uint32_t size;
} DataDirectory;


typedef struct Section {
  char name[8]; // 0x0000
  uint32_t virtualSize; // 0x0008
  uint32_t virtualAddress; // 0x000C
  uint32_t sizeOfRawData; // 0x00010
  uint32_t pointerToRawData; // 0x0014
  uint32_t pointerToRelocations;
  uint32_t pointerToLinenumbers;
  uint16_t numberOfRelocations;
  uint16_t numberOfLinenumbers;
  uint32_t characteristics;
} Section;


typedef struct CLRHeader {
  uint32_t HeaderSize; // 0x0
  uint16_t MajorRuntimeVersion; // 0x4
  uint16_t MinorRuntimeVersion; // 0x6
  uint32_t MetaDataDirectoryAddress;  // 0x8
  uint32_t MetaDataDirectorySize;
  uint32_t Flags;
  uint32_t EntryPointToken;
  uint32_t ResourcesDirectoryAddress;
  uint32_t ResourcesDirectorySize;
  uint32_t StrongNameSignatureAddress;
  uint32_t StrongNameSignatureSize;
  uint32_t CodeManagerTableAddress;
  uint32_t CodeManagerTableSize;
  uint32_t VTableFixupsAddress;
  uint32_t VTableFixupsSize;
  uint32_t ExportAddressTableJumpsAddress;
  uint32_t ExportAddressTableJumpsSize;
  uint32_t ManagedNativeHeaderAddress;
  uint32_t ManagedNativeHeaderSize;
} CLRHeader;

typedef struct MetadataHeader {
  uint32_t Signature; // always 0x424A5342 [42 53 4A 42]
  uint16_t MajorVersion; // always 0x0001 [01 00]
  uint16_t MinorVersion; // always 0x0001 [01 00]
  uint32_t Reserved1; // always 0x00000000 [00 00 00 00]
  uint32_t VersionStringLength;
  char VersionString[12]; // null terminated in file. VersionStringLength includes the null(s) in the length, and also is always rounded up to a multiple of 4.
  uint16_t Flags; // always 0x0000 [00 00]
  uint16_t NumberOfStreams;
} MetadataHeader;

typedef struct StreamHeader {
  uint32_t Offset;
  uint32_t Size;
} StreamHeader;

typedef struct MetadataStreamHeader {
  uint32_t Reserved1;
  uint8_t MajorVersion;
  uint8_t MinorVersion;
  uint8_t OffsetSizeFlags;
  uint8_t Reserved2;
  uint64_t TablesFlags;
  uint64_t SortedTablesFlags;
} MetadataStreamHeader;

typedef struct TableStreamHeader {
  uint32_t Reserved1;   // 0x0
  uint8_t MajorVersion; // 0x4
  uint8_t MinorVersion; // 0x5
  uint8_t HeapSizes;  // 0x6
  uint8_t Reserved2;  // 0x7
  uint64_t MaskValid; // 0x8
  uint64_t MaskSorted;  // 0x10
} TableStreamHeader;

typedef struct ColumnOffsetSize {
  uint32_t size;
  uint16_t offset;
} ColumnOffsetSize;

#define StreamOffsetSizeFlagsString 0x01
#define StreamOffsetSizeFlagsGUID 0x02
#define StreamOffsetSizeFlagsBlob 0x04

#define MetadataTableFlagsModule    1
#define MetadataTableFlagsTypeRef    2
#define MetadataTableFlagsTypeDef    4
#define MetadataTableFlagsReserved1    8
#define MetadataTableFlagsField    16
#define MetadataTableFlagsReserved2    32
#define MetadataTableFlagsMethod    64
#define MetadataTableFlagsReserved3    128
#define MetadataTableFlagsParam    256
#define MetadataTableFlagsInterfaceImpl    512
#define MetadataTableFlagsMemberRef    1024
#define MetadataTableFlagsConstant    2048
#define MetadataTableFlagsCustomAttribute    4096
#define MetadataTableFlagsFieldMarshal    8192
#define MetadataTableFlagsDeclSecurity    16384
#define MetadataTableFlagsClassLayout    32768
#define MetadataTableFlagsFieldLayout    65536
#define MetadataTableFlagsStandAloneSig    131072
#define MetadataTableFlagsEventMap    262144
#define MetadataTableFlagsReserved4    524288
#define MetadataTableFlagsEvent    1048576
#define MetadataTableFlagsPropertyMap    2097152
#define MetadataTableFlagsReserved5    4194304
#define MetadataTableFlagsProperty    8388608
#define MetadataTableFlagsMethodSemantics    16777216
#define MetadataTableFlagsMethodImpl    33554432
#define MetadataTableFlagsModuleRef    67108864
#define MetadataTableFlagsTypeSpec    134217728
#define MetadataTableFlagsImplMap    268435456
#define MetadataTableFlagsFieldRVA    536870912
#define MetadataTableFlagsReserved6    1073741824
#define MetadataTableFlagsReserved7    2147483648
#define MetadataTableFlagsAssembly    4294967296
#define MetadataTableFlagsAssemblyProcessor    8589934592
#define MetadataTableFlagsAssemblyOS    17179869184
#define MetadataTableFlagsAssemblyRef    34359738368
#define MetadataTableFlagsAssemblyRefProcessor    68719476736
#define MetadataTableFlagsAssemblyRefOS    137438953472
#define MetadataTableFlagsFile    274877906944
#define MetadataTableFlagsExportedType    549755813888
#define MetadataTableFlagsManifestResource    1099511627776
#define MetadataTableFlagsNestedClass    2199023255552
#define MetadataTableFlagsGenericParam    4398046511104
#define MetadataTableFlagsMethodSpec    8796093022208
#define MetadataTableFlagsGenericParamConstraint    17592186044416

typedef uint32_t TableIndex;
typedef uint32_t TableRowIndex;
typedef uint32_t TableRowCount;

const static TableIndex TableIndexInvalid = 0xFFFFFFFF;
const static TableRowIndex TableRowIndexInvalid = 0;
const static TableRowIndex TableRowIndexStart = 1;

const static TableIndex TableIndexModule = 0;
const static TableIndex TableIndexTypeRef = 1;
const static TableIndex TableIndexTypeDef = 2;
const static TableIndex TableIndexFieldPointer = 3;
const static TableIndex TableIndexField = 4;
const static TableIndex TableIndexMethodPointer = 5;
const static TableIndex TableIndexMethodDef = 6;
const static TableIndex TableIndexParamPointer = 7;
const static TableIndex TableIndexParam = 8;
const static TableIndex TableIndexInterfaceImpl = 9;
const static TableIndex TableIndexMemberRef = 10;
const static TableIndex TableIndexConstant = 11;
const static TableIndex TableIndexCustomAttribute = 12;
const static TableIndex TableIndexFieldMarshal = 13;
const static TableIndex TableIndexDeclSecurity = 14;
const static TableIndex TableIndexClassLayout = 15;
const static TableIndex TableIndexFieldLayout = 16;
const static TableIndex TableIndexStandAloneSig = 17;
const static TableIndex TableIndexEventMap = 18;
const static TableIndex TableIndexEventPointer = 19;
const static TableIndex TableIndexEvent = 20;
const static TableIndex TableIndexPropertyMap = 21;
const static TableIndex TableIndexPropertyPointer = 22;
const static TableIndex TableIndexProperty = 23;
const static TableIndex TableIndexMethodSemantics = 24;
const static TableIndex TableIndexMethodImpl = 25;
const static TableIndex TableIndexModuleRef = 26;
const static TableIndex TableIndexTypeSpec = 27;
const static TableIndex TableIndexImplMap = 28;
const static TableIndex TableIndexFieldRVA = 29;
const static TableIndex TableIndexUnused6 = 30;
const static TableIndex TableIndexUnused7 = 31;
const static TableIndex TableIndexAssembly = 32;
const static TableIndex TableIndexAssemblyProcessor = 33;
const static TableIndex TableIndexAssemblyOS = 34;
const static TableIndex TableIndexAssemblyRef = 35;
const static TableIndex TableIndexAssemblyRefProcessor = 36;
const static TableIndex TableIndexAssemblyRefOS = 37;
const static TableIndex TableIndexFile = 38;
const static TableIndex TableIndexExportedType = 39;
const static TableIndex TableIndexManifestResource = 40;
const static TableIndex TableIndexNestedClass = 41;
const static TableIndex TableIndexGenericParam = 42;
const static TableIndex TableIndexMethodSpec = 43;
const static TableIndex TableIndexGenericParamConstraint = 44;
const static TableIndex TableIndexUnused8 = 45;
const static TableIndex TableIndexUnused9 = 46;
const static TableIndex TableIndexUnused10 = 47;
/* Portable PDB tables */
const static TableIndex TableIndexDocument = 48;
const static TableIndex TableIndexMethodBody = 49;
const static TableIndex TableIndexLocalScope = 50;
const static TableIndex TableIndexLocalVariable = 51;
const static TableIndex TableIndexImportScope = 52;
const static TableIndex TableIndexStateMachineMethod = 53;
const static TableIndex TableIndexCustomDebugInformation = 54;

const static uint32_t MetadataTableFlagsCount = 45;

const static TableIndex HasCustomAttributeAssociateTableIndexes[] = {
  TableIndexMethodDef,
  TableIndexField,
  TableIndexTypeRef,
  TableIndexTypeDef,
  TableIndexParam,
  TableIndexInterfaceImpl,
  TableIndexMemberRef,
  TableIndexModule,
  TableIndexDeclSecurity,
  TableIndexProperty,
  TableIndexEvent,
  TableIndexStandAloneSig,
  TableIndexModuleRef,
  TableIndexAssembly,
  TableIndexFile,
  TableIndexExportedType,
  TableIndexGenericParam,
  TableIndexGenericParamConstraint,
  TableIndexMethodSpec
};

const static uint32_t HasCustomAttributeAssociateTableIndexesCount = sizeof(HasCustomAttributeAssociateTableIndexes) / sizeof(TableIndex);

const static uint32_t TagBitsTypeDefOrRef = 2;
const static uint32_t TagBitsHasConstant = 2;
const static uint32_t TagBitsHasCustomAttribute = 5;
const static uint32_t TagBitsHasFieldMarshal = 1;
const static uint32_t TagBitsHasDeclSecurity = 2;
const static uint32_t TagBitsMemberRefParent = 3;
const static uint32_t TagBitsHasSemantics = 1;
const static uint32_t TagBitsMethodDefOrRef = 1;
const static uint32_t TagBitsMemberForwarded = 1;
const static uint32_t TagBitsImplementation = 2;
const static uint32_t TagBitsCustomAttributeType = 3;
const static uint32_t TagBitsResolutionScope = 2;
const static uint32_t TagBitsTypeOrMethodDef = 1;

#endif //DOTNETFILEPARSER_COMMON_H
