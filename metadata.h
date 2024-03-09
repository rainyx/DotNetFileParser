//
// Created by admin on 2024/3/7.
//

#ifndef DOTNETFILEPARSER_METADATA_H
#define DOTNETFILEPARSER_METADATA_H

#include "common.h"
#include "stream.h"

#include <string>
#include <vector>
#include <map>
#include <type_traits>

typedef uint32_t BlobIndex;
typedef uint32_t StringIndex;
typedef uint32_t GUIDIndex;

template<uint8_t bitsNum, TableIndex... tableIndexes>
struct CodedIndex {
public:
  const static uint8_t BitsNum = bitsNum;
  constexpr static auto TableIndexes = std::integer_sequence<TableIndex, tableIndexes...>();

  CodedIndex(uint32_t codedIndex): _codedIndex(codedIndex) {};
  CodedIndex(): _codedIndex(0) {};

  TableIndex GetTableIndex() {
    return GetTableIndex(_codedIndex & _mask, 0, tableIndexes...);
  }

  uint32_t GetRowIndex() {
    return _codedIndex >> bitsNum;
  }

private:
  uint32_t _codedIndex;
  constexpr static uint32_t _mask = ((uint32_t)1 << bitsNum) - 1;

  template<class TFirst>
  TableIndex GetTableIndex(uint32_t m, TFirst first) {
    // Overflow.
    if (m == _mask)
      throw "Overflow";
    return m == (_codedIndex & _mask) ? first : TableIndexInvalid;
  }

  template<class TFirst, class... TOther>
  TableIndex GetTableIndex(uint32_t m, TFirst first, TOther... other) {
    if (GetTableIndex(m, first) != TableIndexInvalid)
      return first;
    return GetTableIndex(m + 1, other...);
  }
};

// TypeDefOrRef: 2 bits to encode tag
typedef CodedIndex<2,
  TableIndexTypeDef,
  TableIndexTypeRef,
  TableIndexTypeSpec
> TypeDefOrRefCodedIndex;

// HasConstant: 2 bits to encode tag
typedef CodedIndex<2,
  TableIndexField,
  TableIndexParam,
  TableIndexProperty
> HasConstantCodedIndex;

// HasCustomAttribute: 5 bits to encode tag
typedef CodedIndex<5,
  TableIndexMethodDef,
  TableIndexField,
  TableIndexTypeRef,
  TableIndexTypeDef,
  TableIndexParam,
  TableIndexInterfaceImpl,
  TableIndexMemberRef,
  TableIndexModule,
  TableIndexDeclSecurity,  // Permission
  TableIndexProperty,
  TableIndexEvent,
  TableIndexStandAloneSig,
  TableIndexModuleRef,
  TableIndexTypeSpec,
  TableIndexAssembly,
  TableIndexAssemblyRef,
  TableIndexFile,
  TableIndexExportedType,
  TableIndexManifestResource,
  TableIndexGenericParam,
  TableIndexGenericParamConstraint,
  TableIndexMethodSpec
> HasCustomAttributeCodedIndex;

// HasFieldMarshall: 1 bit to encode tag
typedef CodedIndex<1,
  TableIndexField,
  TableIndexParam
> HasFieldMarshallCodedIndex;

// HasDeclSecurity: 2 bits to encode tag
typedef CodedIndex<2,
  TableIndexTypeDef,
  TableIndexMethodDef,
  TableIndexAssembly
> HasDeclSecurityCodedIndex;

// MemberRefParent: 3 bits to encode tag
typedef CodedIndex<3,
  TableIndexTypeDef,
  TableIndexTypeRef,
  TableIndexModuleRef,
  TableIndexMethodDef,
  TableIndexTypeSpec
> MemberRefParentCodedIndex;

// HasSemantics: 1 bit to encode tag
typedef CodedIndex<1,
  TableIndexEvent,
  TableIndexProperty
> HasSemanticsCodedIndex;

// MethodDefOrRef: 1 bit to encode tag
typedef CodedIndex<1,
  TableIndexMethodDef,
  TableIndexMemberRef
> MethodDefOrRefCodedIndex;

// MemberForwarded: 1 bit to encode tag
typedef CodedIndex<1,
  TableIndexField,
  TableIndexMethodDef
> MemberForwardedCodedIndex;

// Implementation: 2 bits to encode tag
typedef CodedIndex<2,
  TableIndexFile,
  TableIndexAssemblyRef,
  // [!!! NOTE !!!]
  // May II.22.24 ManifestResource : 0x28 not use TableIndexExportedType
  TableIndexExportedType
> ImplementationCodedIndex;

// CustomAttributeType: 3 bits to encode tag
typedef CodedIndex<3,
  TableIndexInvalid,    // not used
  TableIndexInvalid,    // not used
  TableIndexMethodDef,
  TableIndexMemberRef,
  TableIndexInvalid     // not used
> CustomAttributeTypeCodedIndex;

// ResolutionScope: 2 bits to encode tag
typedef CodedIndex<2,
  TableIndexModule,
  TableIndexModuleRef,
  TableIndexAssemblyRef,
  TableIndexTypeRef
> ResolutionScopeCodedIndex;

// TypeOrMethodDef: 1 bit to encode tag
typedef CodedIndex<1,
  TableIndexTypeDef,
  TableIndexMethodDef
> TypeOrMethodDefCodedIndex;


// II.23.1.1 Values for AssemblyHashAlgorithm
enum class AssemblyHashAlgorithm: uint32_t {
  None = 0,
  // (MD5)
  Reserved = 0x8003,
  SHA1 = 0x8004
};

// II.23.1.2 Values for AssemblyFlags
enum class AssemblyFlags: uint32_t {
  PublicKey = 0x0001,
  Retargetable = 0x0100,
  DisableJITcompileOptimizer = 0x4000,
  EnableJITcompileTracking = 0x8000
};

// II.23.1.4 Flags for events [EventAttributes]
enum class EventAttributes: uint32_t {
  SpecialName = 0x2000,
  RTSpecialName = 0x4000
};


// II.23.1.5 Flags for fields [FieldAttributes]
enum class FieldAttributes: uint32_t {
  FieldAccessMask = 0x0007,
  CompilerControlled = 0x0000,
  Private = 0x0001,
  FamANDAssem = 0x0002,
  Assembly = 0x0003,
  Family = 0x0004,
  FamORAssem = 0x0005,
  Public = 0x0006,
  Static = 0x0010,
  InitOnly = 0x0020,
  Literal = 0x0040,
  NotSerialized = 0x0080,
  SpecialName = 0x0200,
  // Interop Attributes
  PInvokeImpl = 0x2000,
  // Additional flags
  RTSpecialName = 0x0400,
  HasFieldMarshal = 0x1000,
  HasDefault = 0x8000,
  HasFieldRVA = 0x0100
};

// II.23.1.6 Flags for files [FileAttributes]
enum class FileAttributes {
  ContainsMetaData = 0x0000,
  ContainsNoMetaData = 0x0001
};

// II.23.1.7 Flags for Generic Parameters [GenericParamAttributes]
enum class GenericParamAttributes: uint32_t {
  VarianceMask = 0x0003,
  None = 0x0000,
  Covariant = 0x0001,
  Contravariant = 0x0002,
  SpecialConstraintMask = 0x001C,
  ReferenceTypeConstraint = 0x0004,
  NotNullableValueTypeConstraint = 0x0008,
  DefaultConstructorConstraint = 0x0010
};

// II.23.1.8 Flags for ImplMap [PInvokeAttributes]
enum class PInvokeAttributes: uint32_t {
  NoMangle = 0x0001,
  // Character set
  CharSetMask = 0x0006,
  CharSetNotSpec = 0x0000,
  CharSetAnsi = 0x0002,
  CharSetUnicode = 0x0004,
  CharSetAuto = 0x0006,
  SupportsLastError = 0x0040,
  // Calling convention
  CallConvMask = 0x0700,
  CallConvPlatformApi = 0x0100,
  CallConvCdecl = 0x0200,
  CallConvStdCall = 0x0300,
  CallConvThisCall = 0x0400,
  CallConvFastCall = 0x0500
  // end
};

// II.23.1.9 Flags for ManifestResource [ManifestResourceAttributes]
enum class ManifestResourceAttributes: uint32_t {
  VisibilityMask = 0x0007,
  Public = 0x0001,
  Private = 0x0002
};

// II.23.1.10 Flags for methods [MethodAttributes]
enum class MethodAttributes: uint32_t {
  MemberAccessMask = 0x0007,
  CompilerControlled = 0x0000,
  Private = 0x0001,
  FAmANDAssem = 0x0002,
  Assem = 0x0003,
  Family = 0x0004,
  FamORAssem = 0x0005,
  Public = 0x0006,
  // end access mask
  Static = 0x0010,
  Final = 0x0020,
  Virtual = 0x0040,
  HideBySig = 0x0080,
  VtableLayoutMask = 0x0100,
  ResultSlot = 0x0000,
  NewSlot = 0x0100,
  // end vtable layout mask.
  Strict = 0x0200,
  Abstract = 0x0400,
  SpecialName = 0x0800,
  //Interop attributes
  PInvokeImpl = 0x2000,
  UnmanagedExport = 0x0008,
  // Additional flags
  RTSpecialName = 0x1000,
  HasSecurity = 0x4000,
  RequireSecObject = 0x8000
};

// II.23.1.11 Flags for methods [MethodImplAttributes]
enum class MethodImplAttributes: uint32_t {
  CodeTypeMask = 0x0003,
  IL = 0x0000,
  Native = 0x0001,
  OPTIL = 0x0002,
  Runtime = 0x0003,
  // end
  ManagedMask = 0x0004,
  Unmanaged = 0x0004,
  Managed = 0000,
  // end
  // Implementation info and interop
  ForwardRef = 0x0010,
  PreserveSig = 0x0080,
  InternalCall = 0x1000,
  Synchronized = 0x0020,
  NoInlining = 0x0008,
  // Range check value
  MaxMethodImplVal = 0xffff,
  NoOptimization = 0x0040
};

// II.23.1.12 Flags for MethodSemantics [MethodSemanticsAttributes]
enum class MethodSemanticsAttributes: uint32_t {
  Setter = 0x0001,
  Getter = 0x0002,
  Other = 0x0004,
  AddOn = 0x0008,
  RemoveOn = 0x0010,
  Fire = 0x0020
};

// II.23.1.13 Flags for params [ParamAttributes]
enum class ParamAttributes: uint32_t {
  In = 0x0001,
  Out = 0x0002,
  Optional = 0x0003,
  HasDefault = 0x1000,
  HasFieldMarshal = 0x2000,
  Unused = 0xcfe0
};

// II.23.1.14 Flags for properties [PropertyAttributes]
enum class PropertyAttributes: uint32_t {
  SpecialName = 0x0200,
  RTSpecialName = 0x0400,
  HasDefault = 0x1000,
  Unused = 0xe9ff
};

// II.23.1.15 Flags for types [TypeAttributes]
enum class TypeAttributes: uint32_t {
  VisibilityMask = 0x00000007,
  NotPublic = 0x00000000,
  Public = 0x00000001,
  NestedPublic = 0x00000002,
  NestedPrivate = 0x00000003,
  NestedFamily = 0x00000004,
  NestedAssembly = 0x00000005,
  NestedFamANDAssem = 0x00000006,
  NestedFamORAssem = 0x00000007,
  // end
  // Class Layout attributes
  LayoutMask = 0x00000018,
  AutoLayout = 0x00000000,
  SequentialLayout = 0x00000008,
  ExplicitLayout = 0x00000010,
  // end
  // Class semantics attributes
  ClassSemanticsMask = 0x00000020,
  Class = 0x00000000,
  Interface = 0x00000020,
  // end
  // Special semantics in addition to class semantics
  Abstract = 0x00000080,
  Sealed = 0x00000100,
  SpecialName = 0x00000400,
  // Implementation Attributes
  Import = 0x00001000,
  Serializable = 0x00002000,
  // String formatting Attributes
  StringFormatMask = 0x00030000,
  AnsiClass = 0x00000000,
  UnicodeClass = 0x00010000,
  AutoClass = 0x00020000,
  CustomFormatClass = 0x00030000,
  // end
  CustomStringFormatMask = 0x00C00000,
  // end
  // Class Initialization Attributes
  BeforeFieldInit = 0x00100000,
  // Additional Flags
  RTSpecialName = 0x00000800,
  HasSecurity = 0x00040000,
  IsTypeForwarder = 0x00200000,
};

// II.23.1.16 Element types used in signatures
// The following table lists the values for ELEMENT_TYPE constants. These are used extensively in metadata signature blobs – see §II.23.2
enum class ELEMENT_TYPE {
  END = 0x00,
  VOID = 0x01,
  BOOLEAN = 0x02,
  CHAR = 0x03,
  I1 = 0x04,
  U1 = 0x05,
  I2 = 0x06,
  U2 = 0x07,
  I4 = 0x08,
  U4 = 0x09,
  I8 = 0x0a,
  U8 = 0x0b,
  R4 = 0x0c,
  R8 = 0x0d,
  STRING = 0x0e,
  PTR = 0x0f,
  BYREF = 0x10,
  VALUETYPE = 0x11,
  CLASS = 0x12,
  VAR = 0x13,
  ARRAY = 0x14,
  GERERICINST = 0x15,
  TYPEDBYREF = 0x16,
  I = 0x18,
  U = 0x19,
  FNPTR = 0x1b,
  OBJECT = 0x1c,
  SZARRAY = 0x1d,
  MVAR = 0x1e,
  CMOD_REQD = 0x1f,
  CMOD_OPT = 0x20,
  INTERNAL = 0x21,
  MODIFIER = 0x40,
  SENTINEL = 0x41,
  PINNED = 0x45
};

// II.23.1.3 Values for Culture
const static char *CultureNames[] {
  "ar-SA", "ar-IQ", "ar-DZ", "ar-MA", "ar-YE", "ar-SY", "ar-KW", "ar-AE", "bg-BG", "ca-ES", "zh-HK", "zh-SG", "da-DK", "de-DE", "de-LU", "de-LI", "en-GB", "en-AU", "en-IE", "en-ZA", "en-BZ", "en-TT", "es-ES-Ts", "es-MX", "es-CR", "es-PA", "es-CO", "es-PE", "es-CL", "es-UY", "es-SV", "es-HN", "fi-FI", "fr-FR",
  "ar-EG", "ar-LY", "ar-TN", "ar-OM", "ar-JO", "ar-LB", "ar-BH", "ar-QA", "zh-TW", "zh-CN", "zh-MO", "cs-CZ", "de-CH", "de-AT", "el-GR", "en-US", "en-CA", "en-NZ", "en-JM", "en-CB", "en-ZW", "en-PH", "es-ES-Is", "es-GT", "es-DO", "es-VE", "es-AR", "es-EC", "es-PY", "es-BO", "es-NI", "es-PR", "fr-BE", "fr-CA",
  "fr-CH", "fr-LU", "fr-MC", "hu-HU", "is-IS", "it-IT", "ja-JP", "ko-KR", "nl-NL", "nb-NO", "nn-NO", "pl-PL", "pt-PT", "ro-RO", "ru-RU", "lt-sr-SP", "cy-sr-SP", "sk-SK", "sv-SE", "sv-FI", "th-TH", "ur-PK", "id-ID", "uk-UA", "sl-SI", "et-EE", "lv-LV", "fa-IR", "vi-VN", "hy-AM", "cy-az-AZ", "eu-ES", "mk-MK", "ka-GE", "fo-FO", "hi-IN", "ms-BN", "kk-KZ", "ky-KZ", "lt-uz-UZ", "cy-uz-UZ", "tt-TA", "gu-IN", "ta-IN", "te-IN", "mr-IN", "sa-IN", "mn-MN", "kok-IN", "syr-SY", "div-MV",
  "he-IL", "it-CH", "nl-BE", "pt-BR", "hr-HR", "sq-AL", "tr-TR", "be-BY", "lt-LT", "lt-az-AZ", "af-ZA", "ms-MY", "sw-KE", "pa-IN", "kn-IN", "gl-ES",
};

const static uint32_t CultureNamesCount = sizeof(CultureNames) / sizeof(char *);


// II.22.3 AssemblyOS : 0x22
struct AssemblyOSTableRow {
  // OSPlatformID (a 4-byte constant)
  uint32_t OSPlatformId;
  // OSMajorVersion (a 4-byte constant)
  uint32_t OSMajorVersion;
  // OSMinorVersion (a 4-byte constant)
  uint32_t OSMinorVersion;
};

// II.22.4 AssemblyProcessor : 0x21
struct AssemblyProcessorTableRow {
  // Processor (a 4-byte constant)
  uint32_t Processor;
};

// II.22.5 AssemblyRef : 0x23
struct AssemblyRefTableRow {
  // MajorVersion, MinorVersion, BuildNumber, RevisionNumber (each being 2-byte constants)
  uint16_t MajorVersion;
  uint16_t MinorVersion;
  uint16_t BuildNumber;
  uint16_t ReversionNumber;
  // Flags (a 4-byte bitmask of type AssemblyFlags, §II.23.1.2)
  AssemblyFlags Flags;
  // PublicKeyOrToken (an index into the Blob heap, indicating the public key or token
  // that identifies the author of this Assembly)
  BlobIndex PublicKeyOrToken;
  // Name (an index into the String heap)
  StringIndex Name;
  // Culture (an index into the String heap)
  StringIndex Culture;
  // HashValue (an index into the Blob heap)
  BlobIndex HashValue;
};

// II.22.6 AssemblyRefOS : 0x25
struct AssemblyRefOSTableRow {
  // OSPlatformId (a 4-byte constant)
  uint32_t OSPlatformId;
  // OSMajorVersion (a 4-byte constant)
  uint32_t OSMajorVersion;
  // OSMinorVersion (a 4-byte constant)
  uint32_t OSMinorVersion;
  // AssemblyRef (an index into the AssemblyRef table)
  TableIndex AssemblyRef;
};

// II.22.7 AssemblyRefProcessor : 0x24
struct AssemblyRefProcessorTableRow {
  // Processor (a 4-byte constant)
  uint32_t Processor;
  // AssemblyRef (an index into the AssemblyRef table)
  TableIndex AssemblyRef;
};

// II.22.8 ClassLayout : 0x0F
struct ClassLayoutTableRow {
  // PackingSize (a 2-byte constant)
  uint16_t PackingSize;
  // ClassSize (a 4-byte constant)
  uint32_t ClassSize;
  // Parent (an index into the TypeDef table)
  TableIndex Parent;
};

// II.22.9 Constant : 0x0B
struct ConstantTableRow {
  // Type (a 1-byte constant, followed by a 1-byte padding zero); see §II.23.1.16 .
  // The encoding of Type for the nullref value for FieldInit in ilasm (§II.16.2) is ELEMENT_TYPE_CLASS with a Value of a 4-byte zero.
  // Unlike uses of ELEMENT_TYPE_CLASS in signatures, this one is not followed by a type token.
  uint16_t Type;
  // Parent (an index into the Param, Field, or Property table; more precisely, a HasConstant (§II.24.2.6) coded index)
  HasConstantCodedIndex Parent;
  // Value (an index into the Blob heap)
  BlobIndex Value;
};

// II.22.10 CustomAttribute : 0x0C
struct CustomAttributeTableRow {
  // Parent (an index into a metadata table that has an associated HasCustomAttribute
  // (§II.24.2.6) coded index).
  HasCustomAttributeCodedIndex Parent;
  // Type (an index into the MethodDef or MemberRef table; more precisely, a
  // CustomAttributeType (§II.24.2.6) coded index).
  CustomAttributeTypeCodedIndex Type;
  // Value (an index into the Blob heap)
  BlobIndex Value;
};

// II.22.11 DeclSecurity : 0x0E
struct DeclSecurityTableRow {
  uint16_t Action;
  // Parent (an index into the TypeDef, MethodDef, or Assembly table; more precisely, a HasDeclSecurity (§II.24.2.6) coded index)
  HasDeclSecurityCodedIndex Parent;
  // PermissionSet (an index into the Blob heap)
  BlobIndex PermissionSet;
};

// II.22.12 EventMap : 0x12
struct EventMapTableRow {
  // Parent (an index into the TypeDef table)
  TableIndex Parent;
  // EventList (an index into the Event table). It marks the first of a contiguous run of Events owned by this Type. That run continues to the smaller of:
  //  o the last row of the Event table
  //  o the next run of Events, found by inspecting the EventList of the next row
  //    in the EventMap table
  TableIndex EventList;
};

// II.22.13 Event : 0x14
struct EventTableRow {
  // EventFlags (a 2-byte bitmask of type EventAttributes, §II.23.1.4)
  EventAttributes EventFlags;
  StringIndex Name;
  // EventType (an index into a TypeDef, a TypeRef, or TypeSpec table; more precisely,
  // a TypeDefOrRef (§II.24.2.6) coded index) (This corresponds to the Type of the Event; it is not the Type that owns this event.)
  TypeDefOrRefCodedIndex EventType;
};

// II.22.14 ExportedType : 0x27
struct ExportedTypeTableRow {
  // Flags (a 4-byte bitmask of type TypeAttributes, §II.23.1.15)
  TypeAttributes Flags;
  // TypeDefId (a 4-byte index into a TypeDef table of another module in this Assembly).
  // This column is used as a hint only. If the entry in the target TypeDef table matches the TypeName and TypeNamespace entries in this table,
  // resolution has succeeded. But if there is a mismatch, the CLI shall fall back to a search of the target TypeDef table.
  // Ignored and should be zero if Flags has IsTypeForwarder set.
  TableIndex TypeDefId;
  StringIndex TypeName;
  StringIndex TypeNamespace;
  // Implementation. This is an index (more precisely, an Implementation (§II.24.2.6) coded index) into either of the following tables:
  //  o File table, where that entry says which module in the current assembly holds the TypeDef
  //  o ExportedType table, where that entry is the enclosing Type of the current nested Type
  //  o AssemblyRef table, where that entry says in which assembly the type may now be found (Flags must have the IsTypeForwarder flag set).
  ImplementationCodedIndex Implementation;
};

// II.22.15 Field : 0x04
struct FieldTableRow {
  // Flags (a 2-byte bitmask of type FieldAttributes, §II.23.1.5)
  FieldAttributes Flags;
  StringIndex Name;
  // Signature (an index into the Blob heap)
  BlobIndex Signature;
};

// II.22.16 FieldLayout : 0x10
struct FieldLayoutTableRow {
  // Offset (a 4-byte constant)
  uint32_t Offset;
  // Field (an index into the Field table)
  TableIndex Field;
};

// II.22.17 FieldMarshal : 0x0D
struct FieldMarshalTableRow {
  // Parent (an index into Field or Param table; more precisely, a HasFieldMarshal
  // (§II.24.2.6) coded index)
  HasFieldMarshallCodedIndex Parent;
  // NativeType (an index into the Blob heap)
  BlobIndex NativeType;
};

// II.22.18 FieldRVA : 0x1D
struct FieldRVATableRow {
  // RVA (a 4-byte constant)
  uint32_t RVA;
  // Field (an index into Field table)
  TableIndex Field;
};

// II.22.19 File : 0x26
struct FileTableRow {
  // Flags (a 4-byte bitmask of type FileAttributes, §II.23.1.6)
  FileAttributes Flags;
  StringIndex Name;
  BlobIndex HashValue;
};

// II.22.20 GenericParam : 0x2A
struct GenericParamTableRow {
  // Number (the 2-byte index of the generic parameter, numbered left-to-right, from zero)
  uint16_t Number;
  // Flags (a 2-byte bitmask of type GenericParamAttributes, §II.23.1.7)
  GenericParamAttributes Flags;
  // Owner (an index into the TypeDef or MethodDef table,
  // specifying the Type or Method to which this generic parameter applies; more precisely, a TypeOrMethodDef (§II.24.2.6) coded index)
  TypeOrMethodDefCodedIndex Owner;
  // Name (a non-null index into the String heap, giving the name for the generic parameter.
  // This is purely descriptive and is used only by source language compilers and by Reflection)
  StringIndex Name;
};

// II.22.21 GenericParamConstraint : 0x2C
struct GenericParamConstraintTableRow {
  // Owner (an index into the GenericParam table, specifying to which generic
  // parameter this row refers)
  TableIndex Owner;
  // Constraint (an index into the TypeDef, TypeRef, or TypeSpec tables, specifying from which class this generic parameter is constrained to derive;
  // or which interface this generic parameter is constrained to implement; more precisely, a TypeDefOrRef (§II.24.2.6) coded index)
  TypeDefOrRefCodedIndex Constraint;
};

// II.22.22 ImplMap : 0x1C
struct ImplMapTableRow {
  // MappingFlags (a 2-byte bitmask of type PInvokeAttributes, §23.1.8)
  PInvokeAttributes MappingFlags;
  // MemberForwarded (an index into the Field or MethodDef table; more precisely,
  // a MemberForwarded (§II.24.2.6) coded index). However, it only ever indexes the MethodDef table,
  // since Field export is not supported.
  MemberForwardedCodedIndex MemberForwarded;
  StringIndex ImportName;
  // ImportScope (an index into the ModuleRef table)
  TableIndex ImportScope;
};

// II.22.23 InterfaceImpl : 0x09
struct InterfaceImplTableRow {
  // Class (an index into the TypeDef table)
  TableIndex Class;
  // Interface (an index into the TypeDef, TypeRef, or TypeSpec table; more precisely, a TypeDefOrRef (§II.24.2.6) coded index)
  TypeDefOrRefCodedIndex Interface;
};

// II.22.24 ManifestResource : 0x28
struct ManifestResourceTableRow {
  uint32_t Offset;
  // Flags (a 4-byte bitmask of type ManifestResourceAttributes, §II.23.1.9)
  ManifestResourceAttributes Flags;
  StringIndex Name;
  // Implementation (an index into a File table, a AssemblyRef table, or null; more precisely, an Implementation (§II.24.2.6) coded index)
  ImplementationCodedIndex Implementation;
};

// II.22.25 MemberRef : 0x0A
struct MemberRefTableRow {
  // Class (an index into the MethodDef, ModuleRef,TypeDef, TypeRef, or TypeSpec tables; more precisely, a MemberRefParent (§II.24.2.6) coded index)
  MemberRefParentCodedIndex Class;
  StringIndex Name;
  BlobIndex Signature;
};

// II.22.26 MethodDef : 0x06
struct MethodDefTableRow {
  // RVA (a 4-byte constant)
  uint32_t RVA;
  // ImplFlags (a 2-byte bitmask of type MethodImplAttributes, §II.23.1.10)
  MethodImplAttributes ImplFlags;
  // Flags (a 2-byte bitmask of type MethodAttributes, §II.23.1.10)
  MethodAttributes Flags;
  StringIndex Name;
  BlobIndex Signature;
  // ParamList (an index into the Param table). It marks the first of a contiguous run of Parameters owned by this method. The run continues to the smaller of:
  //  o the last row of the Param table
  //  o the next run of Parameters, found by inspecting the ParamList of the next
  //    row in the MethodDef table
  TableIndex ParamList;
};

// II.22.27 MethodImpl : 0x19
struct MethodImplTableRow {
  // Class (an index into the TypeDef table)
  TableIndex Class;
  // MethodBody (an index into the MethodDef or MemberRef table; more precisely, a
  // MethodDefOrRef (§II.24.2.6) coded index)
  MethodDefOrRefCodedIndex MethodBody;
  // MethodDeclaration (an index into the MethodDef or MemberRef table; more
  // precisely, a MethodDefOrRef (§II.24.2.6) coded index)
  MethodDefOrRefCodedIndex MethodDeclaration;
};

// II.22.28 MethodSemantics : 0x18
struct MethodSemanticsTableRow {
  // Semantics (a 2-byte bitmask of type MethodSemanticsAttributes, §II.23.1.12)
  uint16_t Semantics;
  // Method (an index into the MethodDef table)
  TableIndex Method;
  // Association (an index into the Event or Property table; more precisely, a HasSemantics (§II.24.2.6) coded index)
  HasSemanticsCodedIndex Association;
};

// II.22.29 MethodSpec : 0x2B
struct MethodSpecTableRow {
  // Method (an index into the MethodDef or MemberRef table, specifying to which generic method this row refers;
  // that is, which generic method this row is an instantiation of; more precisely, a MethodDefOrRef (§II.24.2.6) coded index)
  MethodDefOrRefCodedIndex Method;
  // Instantiation (an index into the Blob heap (§II.23.2.15), holding the signature of this instantiation)
  BlobIndex Instantiation;
};

// II.22.2
struct AssemblyTableRow {
  // HashAlgId (a 4-byte constant of type AssemblyHashAlgorithm, §II.23.1.1)
  uint32_t HashAlgId;
  // MajorVersion, MinorVersion, BuildNumber, RevisionNumber (each being 2-byte constants)
  uint16_t MajorVersion;
  uint16_t MinorVersion;
  uint16_t BuildNumber;
  uint16_t RevisionNumber;
  // Flags (a 4-byte bitmask of type AssemblyFlags, §II.23.1.2)
  AssemblyFlags Flags;
  BlobIndex PublicKey;
  StringIndex Name;
  StringIndex Culture;
};

// II.22.30 Module : 0x00
struct ModuleTableRow {
  // Generation (a 2-byte value, reserved, shall be zero)
  uint16_t Generation;
  StringIndex Name;
  // Mvid (an index into the Guid heap; simply a Guid used to distinguish between two versions of the same module)
  GUIDIndex Mvid;
  // EncId (an index into the Guid heap; reserved, shall be zero)
  GUIDIndex EncId;
  // EncBaseId (an index into the Guid heap; reserved, shall be zero)
  GUIDIndex EncBaseId;
};

// II.22.31 ModuleRef : 0x1A
struct ModuleRefTableRow {
  // Name (an index into the String heap)
  StringIndex Name;
};

// II.22.32 NestedClass : 0x29
struct NestedClassTableRow {
  // NestedClass (an index into the TypeDef table)
  TableIndex NestedClass;
  // EnclosingClass (an index into the TypeDef table)
  TableIndex EnclosingClass;
};

// II.22.33 Param : 0x08
struct ParamTableRow {
  // Flags (a 2-byte bitmask of type ParamAttributes, §II.23.1.13)
  ParamAttributes Flags;
  // Sequence (a 2-byte constant)
  uint16_t Sequence;
  // Name (an index into the String heap)
  StringIndex Name;
};

// II.22.34 Property : 0x17
struct PropertyTableRow {
  // Flags (a 2-byte bitmask of type PropertyAttributes, §II.23.1.14)
  PropertyAttributes Flags;
  // Name (an index into the String heap)
  StringIndex Name;
  // Type (an index into the Blob heap) (The name of this column is misleading.
  // It does not index a TypeDef or TypeRef table—instead it indexes the signature in the Blob heap of the Property)
  BlobIndex Type;
};

// II.22.35 PropertyMap : 0x15
struct PropertyMapTableRow {
  // Parent (an index into the TypeDef table)
  TableIndex Parent;
  // PropertyList (an index into the Property table). It marks the first of a contiguous run of Properties owned by Parent. The run continues to the smaller of:
  //  o the last row of the Property table
  //  o the next run of Properties, found by inspecting the PropertyList of the
  //    next row in this PropertyMap table
  TableIndex PropertyList;
};

// II.22.36 StandAloneSig : 0x11
struct StandAloneSigTableRow {
  // Signature (an index into the Blob heap)
  BlobIndex Signature;
};

// II.22.37 TypeDef : 0x02
struct TypeDefTableRow {
  // Flags (a 4-byte bitmask of type TypeAttributes, §II.23.1.15)
  TypeAttributes Flags;
  // TypeName (an index into the String heap)
  StringIndex TypeName;
  // TypeNamespace (an index into the String heap)
  StringIndex TypeNamespace;
  // Extends (an index into the TypeDef, TypeRef, or TypeSpec table; more precisely, a TypeDefOrRef (§II.24.2.6) coded index)
  TypeDefOrRefCodedIndex Extends;
  // FieldList (an index into the Field table; it marks the first of a contiguous run of Fields owned by this Type). The run continues to the smaller of:
  //    o the last row of the Field table
  TableIndex FieldList;
  // MethodList (an index into the MethodDef table; it marks the first of a continguous
  //   run of Methods owned by this Type). The run continues to the smaller of:
  //   o the last row of the MethodDef table
  TableIndex MethodList;
};

// II.22.38 TypeRef : 0x01
struct TypeRefTableRow {
  // ResolutionScope (an index into a Module, ModuleRef, AssemblyRef or TypeRef table,
  // or null; more precisely, a ResolutionScope (§II.24.2.6) coded index)
  ResolutionScopeCodedIndex ResolutionScope;
  // TypeName (an index into the String heap)
  StringIndex TypeName;
  // TypeNamespace (an index into the String heap)
  StringIndex TypeNamespace;
};

// II.22.39 TypeSpec : 0x1B
struct TypeSpecTableRow {
  // Signature (index into the Blob heap, where the blob is formatted as specified
  // in §II.23.2.14)
  BlobIndex Signature;
};

struct FieldPointerTableRow {
  TableIndex Field;
};

struct MethodPointerTableRow {
  TableIndex Method;
};

struct ParamPointerTableRow {
  TableIndex Param;
};

struct EventPointerTableRow {
  TableIndex Event;
};

struct PropertyPointerTableRow {
  TableIndex Property;
};

template<class TRow>
struct Table {
  std::vector<TRow> _rows;

  void AddRow(const TRow &&row) {
    _rows.push_back(row);
  }

  void AddRow(const TRow &row) {
    _rows.push_back(row);
  }

  const TRow &GetRowAt(uint32_t i) const {
    return _rows[i];
  }

  TRow &GetRowAt(uint32_t i) {
    return _rows[i];
  }

  uint32_t GetRowsCount() const {
    return _rows.size();
  }
};

typedef Table<ModuleTableRow> ModuleTable;
typedef Table<TypeRefTableRow> TypeRefTable;
typedef Table<TypeDefTableRow> TypeDefTable;
typedef Table<FieldPointerTableRow> FieldPointerTable;
typedef Table<FieldTableRow> FieldTable;
typedef Table<MethodPointerTableRow> MethodPointerTable;
typedef Table<MethodDefTableRow> MethodDefTable;
typedef Table<ParamPointerTableRow> ParamPointerTable;
typedef Table<ParamTableRow> ParamTable;
typedef Table<InterfaceImplTableRow> InterfaceImplTable;
typedef Table<MemberRefTableRow> MemberRefTable;
typedef Table<ConstantTableRow> ConstantTable;
typedef Table<CustomAttributeTableRow> CustomAttributeTable;
typedef Table<FieldMarshalTableRow> FieldMarshalTable;
typedef Table<DeclSecurityTableRow> DeclSecurityTable;
typedef Table<ClassLayoutTableRow> ClassLayoutTable;
typedef Table<FieldLayoutTableRow> FieldLayoutTable;
typedef Table<StandAloneSigTableRow> StandAloneSigTable;
typedef Table<EventMapTableRow> EventMapTable;
typedef Table<EventPointerTableRow> EventPointerTable;
typedef Table<EventTableRow> EventTable;
typedef Table<PropertyMapTableRow> PropertyMapTable;
typedef Table<PropertyPointerTableRow> PropertyPointerTable;
typedef Table<PropertyTableRow> PropertyTable;
typedef Table<MethodSemanticsTableRow> MethodSemanticsTable;
typedef Table<MethodImplTableRow> MethodImplTable;
typedef Table<ModuleRefTableRow> ModuleRefTable;
typedef Table<TypeSpecTableRow> TypeSpecTable;
typedef Table<ImplMapTableRow> ImplMapTable;
typedef Table<FieldRVATableRow> FieldRVATable;
typedef Table<AssemblyTableRow> AssemblyTable;
typedef Table<AssemblyProcessorTableRow> AssemblyProcessorTable;
typedef Table<AssemblyOSTableRow> AssemblyOSTable;
typedef Table<AssemblyRefTableRow> AssemblyRefTable;
typedef Table<AssemblyRefProcessorTableRow> AssemblyRefProcessorTable;
typedef Table<AssemblyRefOSTableRow> AssemblyRefOSTable;
typedef Table<FileTableRow> FileTable;
typedef Table<ExportedTypeTableRow> ExportedTypeTable;
typedef Table<ManifestResourceTableRow> ManifestResourceTable;
typedef Table<NestedClassTableRow> NestedClassTable;
typedef Table<GenericParamTableRow> GenericParamTable;
typedef Table<MethodSpecTableRow> MethodSpecTable;
typedef Table<GenericParamConstraintTableRow> GenericParamConstraintTable;

struct Stream {
  uint32_t Offset;
  uint32_t Size;
  const char *Name;
  const uint8_t *Data;
};

struct TableMeta {
  const uint8_t *data;
  uint32_t RowDataSize;
  uint32_t RowsNum;
  bool Valid;
  bool Sorted;
};

struct Metadata {
  std::vector<Stream> _streams;
  std::vector<TableMeta> _tableMetas;

  ModuleTable _moduleTable;
  TypeRefTable _typeRefTable;
  TypeDefTable _typeDefTable;
  FieldPointerTable _fieldPointerTable;
  FieldTable _fieldTable;
  MethodPointerTable _methodPointerTable;
  MethodDefTable _methodDefTable;
  ParamPointerTable _paramPointerTable;
  ParamTable _paramTable;
  InterfaceImplTable _interfaceImplTable;
  MemberRefTable _memberRefTable;
  ConstantTable _constantTable;
  CustomAttributeTable _customAttributeTable;
  FieldMarshalTable _fieldMarshalTable;
  DeclSecurityTable _declSecurityTable;
  ClassLayoutTable _classLayoutTable;
  FieldLayoutTable _fieldLayoutTable;
  StandAloneSigTable _standAloneSigTable;
  EventMapTable _eventMapTable;
  EventPointerTable _eventPointerTable;
  EventTable _eventTable;
  PropertyMapTable _propertyMapTable;
  PropertyPointerTable _propertyPointerTable;
  PropertyTable _propertyTable;
  MethodSemanticsTable _methodSemanticsTable;
  MethodImplTable _methodImplTable;
  ModuleRefTable _moduleRefTable;
  TypeSpecTable _typeSpecTable;
  ImplMapTable _implMapTable;
  FieldRVATable _fieldRVATable;
  AssemblyTable _assemblyTable;
  AssemblyProcessorTable _assemblyProcessorTable;
  AssemblyOSTable _assemblyOSTable;
  AssemblyRefTable _assemblyRefTable;
  AssemblyRefProcessorTable _assemblyRefProcessorTable;
  AssemblyRefOSTable _assemblyRefOSTable;
  FileTable _fileTable;
  ExportedTypeTable _exportedTypeTable;
  ManifestResourceTable _manifestResourceTable;
  NestedClassTable _nestedClassTable;
  GenericParamTable _genericParamTable;
  MethodSpecTable _methodSpecTable;
  GenericParamConstraintTable _genericParamConstraintTable;

  const ModuleTable & GetModuleTable() const { return _moduleTable; }
  const TypeRefTable & GetTypeRefTable() const { return _typeRefTable; }
  const TypeDefTable & GetTypeDefTable() const { return _typeDefTable; }
  const FieldPointerTable & GetFieldPointerTable() const { return _fieldPointerTable; }
  const FieldTable & GetFieldTable() const { return _fieldTable; }
  const MethodPointerTable & GetMethodPointerTable() const { return _methodPointerTable; }
  const MethodDefTable & GetMethodDefTable() const { return _methodDefTable; }
  const ParamPointerTable & GetParamPointerTable() const { return _paramPointerTable; }
  const ParamTable & GetParamTable() const { return _paramTable; }
  const InterfaceImplTable & GetInterfaceImplTable() const { return _interfaceImplTable; }
  const MemberRefTable & GetMemberRefTable() const { return _memberRefTable; }
  const ConstantTable & GetConstantTable() const { return _constantTable; }
  const CustomAttributeTable & GetCustomAttributeTable() const { return _customAttributeTable; }
  const FieldMarshalTable & GetFieldMarshalTable() const { return _fieldMarshalTable; }
  const DeclSecurityTable & GetDeclSecurityTable() const { return _declSecurityTable; }
  const ClassLayoutTable & GetClassLayoutTable() const { return _classLayoutTable; }
  const FieldLayoutTable & GetFieldLayoutTable() const { return _fieldLayoutTable; }
  const StandAloneSigTable & GetStandAloneSigTable() const { return _standAloneSigTable; }
  const EventMapTable & GetEventMapTable() const { return _eventMapTable; }
  const EventPointerTable & GetEventPointerTable() const { return _eventPointerTable; }
  const EventTable & GetEventTable() const { return _eventTable; }
  const PropertyMapTable & GetPropertyMapTable() const { return _propertyMapTable; }
  const PropertyPointerTable & GetPropertyPointerTable() const { return _propertyPointerTable; }
  const PropertyTable & GetPropertyTable() const { return _propertyTable; }
  const MethodSemanticsTable & GetMethodSemanticsTable() const { return _methodSemanticsTable; }
  const MethodImplTable & GetMethodImplTable() const { return _methodImplTable; }
  const ModuleRefTable & GetModuleRefTable() const { return _moduleRefTable; }
  const TypeSpecTable & GetTypeSpecTable() const { return _typeSpecTable; }
  const ImplMapTable & GetImplMapTable() const { return _implMapTable; }
  const FieldRVATable & GetFieldRVATable() const { return _fieldRVATable; }
  const AssemblyTable & GetAssemblyTable() const { return _assemblyTable; }
  const AssemblyProcessorTable & GetAssemblyProcessorTable() const { return _assemblyProcessorTable; }
  const AssemblyOSTable & GetAssemblyOSTable() const { return _assemblyOSTable; }
  const AssemblyRefTable & GetAssemblyRefTable() const { return _assemblyRefTable; }
  const AssemblyRefProcessorTable & GetAssemblyRefProcessorTable() const { return _assemblyRefProcessorTable; }
  const AssemblyRefOSTable & GetAssemblyRefOSTable() const { return _assemblyRefOSTable; }
  const FileTable & GetFileTable() const { return _fileTable; }
  const ExportedTypeTable & GetExportedTypeTable() const { return _exportedTypeTable; }
  const ManifestResourceTable & GetManifestResourceTable() const { return _manifestResourceTable; }
  const NestedClassTable & GetNestedClassTable() const { return _nestedClassTable; }
  const GenericParamTable & GetGenericParamTable() const { return _genericParamTable; }
  const MethodSpecTable & GetMethodSpecTable() const { return _methodSpecTable; }
  const GenericParamConstraintTable & GetGenericParamConstraintTable() const { return _genericParamConstraintTable; }

  ModuleTable & GetModuleTable() { return _moduleTable; }
  TypeRefTable & GetTypeRefTable() { return _typeRefTable; }
  TypeDefTable & GetTypeDefTable() { return _typeDefTable; }
  FieldPointerTable & GetFieldPointerTable() { return _fieldPointerTable; }
  FieldTable & GetFieldTable() { return _fieldTable; }
  MethodPointerTable & GetMethodPointerTable() { return _methodPointerTable; }
  MethodDefTable & GetMethodDefTable() { return _methodDefTable; }
  ParamPointerTable & GetParamPointerTable() { return _paramPointerTable; }
  ParamTable & GetParamTable() { return _paramTable; }
  InterfaceImplTable & GetInterfaceImplTable() { return _interfaceImplTable; }
  MemberRefTable & GetMemberRefTable() { return _memberRefTable; }
  ConstantTable & GetConstantTable() { return _constantTable; }
  CustomAttributeTable & GetCustomAttributeTable() { return _customAttributeTable; }
  FieldMarshalTable & GetFieldMarshalTable() { return _fieldMarshalTable; }
  DeclSecurityTable & GetDeclSecurityTable() { return _declSecurityTable; }
  ClassLayoutTable & GetClassLayoutTable() { return _classLayoutTable; }
  FieldLayoutTable & GetFieldLayoutTable() { return _fieldLayoutTable; }
  StandAloneSigTable & GetStandAloneSigTable() { return _standAloneSigTable; }
  EventMapTable & GetEventMapTable() { return _eventMapTable; }
  EventPointerTable & GetEventPointerTable() { return _eventPointerTable; }
  EventTable & GetEventTable() { return _eventTable; }
  PropertyMapTable & GetPropertyMapTable() { return _propertyMapTable; }
  PropertyPointerTable & GetPropertyPointerTable() { return _propertyPointerTable; }
  PropertyTable & GetPropertyTable() { return _propertyTable; }
  MethodSemanticsTable & GetMethodSemanticsTable() { return _methodSemanticsTable; }
  MethodImplTable & GetMethodImplTable() { return _methodImplTable; }
  ModuleRefTable & GetModuleRefTable() { return _moduleRefTable; }
  TypeSpecTable & GetTypeSpecTable() { return _typeSpecTable; }
  ImplMapTable & GetImplMapTable() { return _implMapTable; }
  FieldRVATable & GetFieldRVATable() { return _fieldRVATable; }
  AssemblyTable & GetAssemblyTable() { return _assemblyTable; }
  AssemblyProcessorTable & GetAssemblyProcessorTable() { return _assemblyProcessorTable; }
  AssemblyOSTable & GetAssemblyOSTable() { return _assemblyOSTable; }
  AssemblyRefTable & GetAssemblyRefTable() { return _assemblyRefTable; }
  AssemblyRefProcessorTable & GetAssemblyRefProcessorTable() { return _assemblyRefProcessorTable; }
  AssemblyRefOSTable & GetAssemblyRefOSTable() { return _assemblyRefOSTable; }
  FileTable & GetFileTable() { return _fileTable; }
  ExportedTypeTable & GetExportedTypeTable() { return _exportedTypeTable; }
  ManifestResourceTable & GetManifestResourceTable() { return _manifestResourceTable; }
  NestedClassTable & GetNestedClassTable() { return _nestedClassTable; }
  GenericParamTable & GetGenericParamTable() { return _genericParamTable; }
  MethodSpecTable & GetMethodSpecTable() { return _methodSpecTable; }
  GenericParamConstraintTable & GetGenericParamConstraintTable() { return _genericParamConstraintTable; }

  const Stream &GetStreamByName(const std::string &name) const {
    for (const Stream &st : _streams) {
      if (st.Name == name)
        return st;
    }

    throw "No stream with name";
  }

  void AddStream(const Stream &&stream) {
    _streams.push_back(stream);
  }

  void SetTableMeta(TableIndex i, const TableMeta &&meta) {
    _tableMetas[i] = meta;
  }

  void ResizeTableMetas(uint8_t sz) {
    _tableMetas.resize(sz);
  }

  const TableMeta &GetTableMeta(TableIndex i) {
    return _tableMetas[i];
  }

  TableRowCount GetTableRowsCount(TableIndex i) const {
    if (i == TableIndexInvalid)
      return 0;
    return _tableMetas[i].RowsNum;
  }

  // Return if table has rows?
  template<class TIdx, typename = typename std::enable_if_t<std::is_same_v<TableIndex, TIdx>>>
  bool TableHasRows(TIdx i) const {
    return _tableMetas[i].RowsNum > 0;
  }

  // Return if any table has rows?
  template<class TIdx, class...TOtherIdx>
  bool TableHasRows(TIdx i, TOtherIdx... other) const {
    return TableHasRows(i) || TableHasRows(other...);
  }

  const char *GetString(StringIndex i) const;

private:
  std::map<StringIndex, std::string> _strings;
};

struct MetadataReader: BinaryReader {
  uint32_t _streamOffsetSizeFlags;
  const Metadata &_metadata;

  MetadataReader(uint32_t streamOffsetSizeFlags, const Metadata &metadata, const uint8_t *data):
    BinaryReader(data), _streamOffsetSizeFlags(streamOffsetSizeFlags), _metadata(metadata) {}

  StringIndex ReadStringStreamIndex() {
    return ReadStreamIndex(StreamOffsetSizeFlagsString);
  }

  GUIDIndex ReadGuidStreamIndex() {
    return ReadStreamIndex(StreamOffsetSizeFlagsGUID);
  }

  BlobIndex ReadBlobStreamIndex() {
    return ReadStreamIndex(StreamOffsetSizeFlagsBlob);
  }

  uint32_t ReadStreamIndex(uint32_t flag) {
    return (_streamOffsetSizeFlags & flag) != 0 ? Read<uint32_t>() : Read<uint16_t>();
  }

  uint32_t ReadTableIndex(TableIndex i) {
    return _metadata.GetTableRowsCount(i) < 65536 ? Read<uint16_t>() : Read<uint32_t>();
  }

  template<uint8_t bitNums, TableIndex... tableIndexes>
  uint32_t ReadCodedIndex(const std::integer_sequence<TableIndex, tableIndexes...> &) {
    TableRowCount maxCount = std::max({_metadata.GetTableRowsCount(tableIndexes)...});
    return (maxCount << bitNums) < 65535 ? Read<uint16_t>() : Read<uint32_t>();
  }

  template<class TCodedIndex>
  TCodedIndex ReadCodedIndex() {
    uint32_t rawIndex = ReadCodedIndex<TCodedIndex::BitsNum>(TCodedIndex::TableIndexes);
    return TCodedIndex(rawIndex);
  }

  ResolutionScopeCodedIndex ReadResolutionScopeCodedIndex() {
    return ReadCodedIndex<ResolutionScopeCodedIndex>();
  }

  TypeDefOrRefCodedIndex ReadTypeDefOrRefCodedIndex() {
    return ReadCodedIndex<TypeDefOrRefCodedIndex>();
  }

  MethodDefOrRefCodedIndex ReadMethodDefOrRefCodedIndex() {
    return ReadCodedIndex<MethodDefOrRefCodedIndex>();
  }

  ImplementationCodedIndex ReadImplementationCodedIndex() {
    return ReadCodedIndex<ImplementationCodedIndex>();
  }

  MemberForwardedCodedIndex ReadMemberForwardedCodedIndex() {
    return ReadCodedIndex<MemberForwardedCodedIndex>();
  }

  HasSemanticsCodedIndex ReadHasSemanticsCodedIndex() {
    return ReadCodedIndex<HasSemanticsCodedIndex>();
  }

  HasDeclSecurityCodedIndex ReadHasDeclSecurityCodedIndex() {
    return ReadCodedIndex<HasDeclSecurityCodedIndex>();
  }

  HasFieldMarshallCodedIndex ReadHasFieldMarshallCodedIndex() {
    return ReadCodedIndex<HasFieldMarshallCodedIndex>();
  }

  CustomAttributeTypeCodedIndex ReadCustomAttributeTypeCodedIndex() {
    return ReadCodedIndex<CustomAttributeTypeCodedIndex>();
  }

  HasConstantCodedIndex ReadHasConstantCodedIndex() {
    return ReadCodedIndex<HasConstantCodedIndex>();
  }

  MemberRefParentCodedIndex ReadMemberRefParentCodedIndex() {
    return ReadCodedIndex<MemberRefParentCodedIndex>();
  }

  TypeOrMethodDefCodedIndex ReadTypeOrMethodDefCodedIndex() {
    return ReadCodedIndex<TypeOrMethodDefCodedIndex>();
  }

  HasCustomAttributeCodedIndex ReadHasCustomAttributeCodedIndex() {
    return ReadCodedIndex<HasCustomAttributeCodedIndex>();
  }
};

struct MetadataParser {

  const MetadataHeader *_metadataHeader;

  MetadataParser(const MetadataHeader *header): _metadataHeader(header) {}

  Metadata Parse();
  void ReadTables(Metadata &metadata, MetadataReader &metadataReader);

};

#endif //DOTNETFILEPARSER_METADATA_H
