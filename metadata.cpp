//
// Created by admin on 2024/3/7.
//

#include "metadata.h"
#include <iostream>
#include <codecvt>

#include "ww898/utf_converters.hpp"
#include "ww898/utf_sizes.hpp"

using namespace std;

const char *Metadata::GetString(StringIndex i) const {
  const Stream &stringStream = GetStreamByName("#Strings");
  return reinterpret_cast<const char *>(stringStream.Data) + i;
}

Metadata MetadataParser::Parse() {
  Metadata metadata;

  cout << _metadataHeader->Signature << endl;
  cout << _metadataHeader->NumberOfStreams << endl;

  uint32_t numOfStreams = _metadataHeader->NumberOfStreams;
  const StreamHeader *st = reinterpret_cast<const StreamHeader *>(_metadataHeader + 1);
  for (int i=0; i<numOfStreams; ++i) {
    int nameLen = 0;
    BinaryReader nameReader(reinterpret_cast<const uint8_t*>(st + 1));
    // Name is varied.
    const char *name = nameReader.ReadNullTermString(&nameLen);
    if (name == nullptr)
      throw "Unable to read string.";

    nameLen = ALIGN_WITH(nameLen, sizeof(uint32_t));

    Stream stream = {
      .Offset     = st->Offset,
      .Size       = st->Size,
      .Name       = name,
      .Data       = reinterpret_cast<const uint8_t *>(_metadataHeader) + st->Offset
    };
    cout << name << endl;
    metadata.AddStream(std::move(stream));

    // move to next.
    st = reinterpret_cast<const StreamHeader *>(reinterpret_cast<const uint8_t *>(st + 1) + nameLen);
  }

  // Read how many rows of each table.
  const TableStreamHeader *tableHeader = reinterpret_cast<const TableStreamHeader *>(reinterpret_cast<const uint8_t *>(_metadataHeader) + metadata.GetStreamByName("#~").Offset);

  cout << (int)tableHeader->MajorVersion << "." << (int)tableHeader->MinorVersion << endl;

  const TableRowCount *tableRowsCount = reinterpret_cast<const TableRowCount *>(tableHeader + 1);

  metadata.ResizeTableMetas(MetadataTableFlagsCount);
  int tablesCount = 0;
  for (uint64_t i=0; i<MetadataTableFlagsCount; ++i) {
    uint64_t mask = static_cast<uint64_t>(1) << i;
    bool sorted = (tableHeader->MaskSorted & mask) != 0;

    if ((mask & tableHeader->MaskValid) != 0) {
      metadata.SetTableMeta(i, { nullptr, 0, tableRowsCount[tablesCount], true, sorted });
      tablesCount ++;
    } else {
      metadata.SetTableMeta(i, { nullptr, 0, 0, false, sorted });
    }
  }
  const uint8_t *rowPtr = reinterpret_cast<const uint8_t *>(tableHeader + 1) + (tablesCount * sizeof(TableRowCount));

  MetadataReader metadataReader(tableHeader->HeapSizes, metadata, rowPtr);

  ReadTables(metadata, metadataReader);

  return metadata;
}

void MetadataParser::ReadTables(Metadata &metadata, MetadataReader &metadataReader) {
  // Read Module table 0x0.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexModule); ++i) {
    ModuleTableRow row;
    row.Generation = metadataReader.Read<uint16_t>();
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Mvid = metadataReader.ReadGuidStreamIndex();
    row.EncId = metadataReader.ReadGuidStreamIndex();
    row.EncBaseId = metadataReader.ReadGuidStreamIndex();
    metadata.GetModuleTable().AddRow(std::move(row));
  }

  // Read TypeRef table 0x1.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexTypeRef); ++i) {
    TypeRefTableRow row;
    // tables: Module, ModuleRef, AssemblyRef TypeRef
    row.ResolutionScope = metadataReader.ReadResolutionScopeCodedIndex();
    row.TypeName = metadataReader.ReadStringStreamIndex();
    row.TypeNamespace = metadataReader.ReadStringStreamIndex();
    metadata.GetTypeRefTable().AddRow(std::move(row));
  }

  // Read TypeDef table 0x2.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexTypeDef); ++i) {
    TypeDefTableRow row;
    row.Flags = static_cast<TypeAttributes>(metadataReader.Read<uint32_t>());
    row.TypeName = metadataReader.ReadStringStreamIndex();
    row.TypeNamespace = metadataReader.ReadStringStreamIndex();
    // tables: TypeDef, TypeRef, TypeSpec
    row.Extends = metadataReader.ReadTypeDefOrRefCodedIndex();
    row.FieldList = metadataReader.ReadTableIndex(TableIndexField);
    row.MethodList = metadataReader.ReadTableIndex(TableIndexMethodDef);
    metadata.GetTypeDefTable().AddRow(std::move(row));
  }

  // Read FieldPointer table 0x3.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexFieldPointer); ++i) {
    FieldPointerTableRow row;
    row.Field = metadataReader.ReadTableIndex(TableIndexField);
    metadata.GetFieldPointerTable().AddRow(std::move(row));
  }

  // Read Field table 0x4.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexField); ++i) {
    FieldTableRow row;
    row.Flags = static_cast<FieldAttributes>(metadataReader.Read<uint16_t>());
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Signature = metadataReader.ReadBlobStreamIndex();
    metadata.GetFieldTable().AddRow(std::move(row));
  }

  // Read MethodPointer table 0x5.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexMethodPointer); ++i) {
    MethodPointerTableRow row;
    row.Method = metadataReader.ReadTableIndex(TableIndexMethodDef);
    metadata.GetMethodPointerTable().AddRow(std::move(row));
  }

  // Read MethodDef table 0x6.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexMethodDef); ++i) {
    MethodDefTableRow row;
    row.RVA = metadataReader.Read<uint32_t>();
    row.ImplFlags = static_cast<MethodImplAttributes>(metadataReader.Read<uint16_t>());
    row.Flags = static_cast<MethodAttributes>(metadataReader.Read<uint16_t>());
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Signature = metadataReader.ReadBlobStreamIndex();
    row.ParamList = metadataReader.ReadTableIndex(TableIndexParam);
    metadata.GetMethodDefTable().AddRow(std::move(row));
  }

  // Read ParamPointer table 0x7.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexParamPointer); ++i) {
    ParamPointerTableRow row;
    row.Param = metadataReader.ReadTableIndex(TableIndexParam);
    metadata.GetParamPointerTable().AddRow(std::move(row));
  }

  // Read Param table 0x8.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexParam); ++i) {
    ParamTableRow row;
    row.Flags = static_cast<ParamAttributes>(metadataReader.Read<uint16_t>());
    row.Sequence = metadataReader.Read<uint16_t>();
    row.Name = metadataReader.ReadStringStreamIndex();
    metadata.GetParamTable().AddRow(std::move(row));
  }

  // Read InterfaceImpl table 0x9.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexInterfaceImpl); ++i) {
    InterfaceImplTableRow row;
    row.Class = metadataReader.ReadTableIndex(TableIndexTypeDef);
    // tables: TypeDef, TypeRef, TypeSpec
    row.Interface = metadataReader.ReadTypeDefOrRefCodedIndex();
    metadata.GetInterfaceImplTable().AddRow(std::move(row));
  }

  // Read MemberRef table 0xA.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexMemberRef); ++i) {
    MemberRefTableRow row;
    // tables: MethodDef, ModuleRef,TypeDef, TypeRef, TypeSpec
    row.Class = metadataReader.ReadMemberRefParentCodedIndex();
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Signature = metadataReader.ReadBlobStreamIndex();
    metadata.GetMemberRefTable().AddRow(std::move(row));
  }

  // Read Constant table 0xB.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexConstant); ++i) {
    ConstantTableRow row;
    row.Type = metadataReader.Read<uint16_t>();
    // tables: Field, Property
    row.Parent = metadataReader.ReadHasConstantCodedIndex();
    row.Value = metadataReader.ReadBlobStreamIndex();
    metadata.GetConstantTable().AddRow(std::move(row));
  }

  // Read CustomAttribute table 0xC.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexCustomAttribute); ++i) {
    CustomAttributeTableRow row;
    row.Parent = metadataReader.ReadHasCustomAttributeCodedIndex();
    // table: MethodDef, MemberRef
    row.Type = metadataReader.ReadCustomAttributeTypeCodedIndex();
    row.Value = metadataReader.ReadBlobStreamIndex();
    metadata.GetCustomAttributeTable().AddRow(std::move(row));
  }

  // Read FieldMarshal table 0xD.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexFieldMarshal); ++i) {
    FieldMarshalTableRow row;
    // tables: Field, Param
    row.Parent = metadataReader.ReadHasFieldMarshallCodedIndex();
    row.NativeType = metadataReader.ReadBlobStreamIndex();
    metadata.GetFieldMarshalTable().AddRow(std::move(row));
  }

  // Read DeclSecurity table 0xE.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexDeclSecurity); ++i) {
    DeclSecurityTableRow row;
    row.Action = metadataReader.Read<uint16_t>();
    // tables: TypeDef, MethodDef, Assembly
    row.Parent = metadataReader.ReadHasDeclSecurityCodedIndex();
    row.PermissionSet = metadataReader.ReadBlobStreamIndex();
    metadata.GetDeclSecurityTable().AddRow(std::move(row));
  }

  // Read ClassLayout table 0xF.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexClassLayout); ++i) {
    ClassLayoutTableRow row;
    row.PackingSize = metadataReader.Read<uint16_t>();
    row.ClassSize = metadataReader.Read<uint32_t>();
    row.Parent = metadataReader.ReadTableIndex(TableIndexTypeDef);
    metadata.GetClassLayoutTable().AddRow(std::move(row));
  }

  // Read FieldLayout table 0x10.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexFieldLayout); ++i) {
    FieldLayoutTableRow row;
    row.Offset = metadataReader.Read<uint32_t>();
    row.Field = metadataReader.ReadTableIndex(TableIndexField);
    metadata.GetFieldLayoutTable().AddRow(std::move(row));
  }

  // Read StandAloneSig table 0x11.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexStandAloneSig); ++i) {
    StandAloneSigTableRow row;
    row.Signature = metadataReader.ReadBlobStreamIndex();
    metadata.GetStandAloneSigTable().AddRow(std::move(row));
  }

  // Read EventMap table 0x12.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexEventMap); ++i) {
    EventMapTableRow row;
    row.Parent = metadataReader.ReadTableIndex(TableIndexTypeDef);
    row.EventList = metadataReader.ReadTableIndex(TableIndexEvent);
    metadata.GetEventMapTable().AddRow(std::move(row));
  }

  // Read EventPointer table 0x13.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexEventPointer); ++i) {
    EventPointerTableRow row;
    row.Event = metadataReader.ReadTableIndex(TableIndexEvent);
    metadata.GetEventPointerTable().AddRow(std::move(row));
  }

  // Read Event table 0x14.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexEvent); ++i) {
    EventTableRow row;
    row.EventFlags = static_cast<EventAttributes>(metadataReader.Read<uint16_t>());
    row.Name = metadataReader.ReadStringStreamIndex();
    // tables: TypeDef, a TypeRef, TypeSpec
    row.EventType = metadataReader.ReadTypeDefOrRefCodedIndex();
    metadata.GetEventTable().AddRow(std::move(row));
  }

  // Read PropertyMap table 0x15.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexPropertyMap); ++i) {
    PropertyMapTableRow row;
    row.Parent = metadataReader.ReadTableIndex(TableIndexTypeDef);
    row.PropertyList = metadataReader.ReadTableIndex(TableIndexProperty);
    metadata.GetPropertyMapTable().AddRow(std::move(row));
  }

  // Read PropertyPointer table 0x16.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexPropertyPointer); ++i) {
    PropertyPointerTableRow row;
    row.Property = metadataReader.ReadTableIndex(TableIndexProperty);
    metadata.GetPropertyPointerTable().AddRow(std::move(row));
  }

  // Read Property table 0x17.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexProperty); ++i) {
    PropertyTableRow row;
    row.Flags = static_cast<PropertyAttributes>(metadataReader.Read<uint16_t>());
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Type = metadataReader.ReadBlobStreamIndex();
    metadata.GetPropertyTable().AddRow(std::move(row));
  }

  // Read MethodSemantics table 0x18.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexMethodSemantics); ++i) {
    MethodSemanticsTableRow row;
    row.Semantics = metadataReader.Read<uint16_t>();
    row.Method = metadataReader.ReadTableIndex(TableIndexMethodDef);
    // tables: Event, Property
    row.Association = metadataReader.ReadHasSemanticsCodedIndex();
    metadata.GetMethodSemanticsTable().AddRow(std::move(row));
  }

  // Read MethodImpl table 0x19.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexMethodImpl); ++i) {
    MethodImplTableRow row;
    row.Class = metadataReader.ReadTableIndex(TableIndexTypeDef);
    // tables: MethodDef, MemberRef
    row.MethodBody = metadataReader.ReadMethodDefOrRefCodedIndex();
    // tables: MethodDef, MemberRef
    row.MethodDeclaration = metadataReader.ReadMethodDefOrRefCodedIndex();
    metadata.GetMethodImplTable().AddRow(std::move(row));
  }

  // Read ModuleRef table 0x1A.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexModuleRef); ++i) {
    ModuleRefTableRow row;
    row.Name = metadataReader.ReadStringStreamIndex();
    metadata.GetModuleRefTable().AddRow(std::move(row));
  }

  // Read TypeSpec table 0x1B.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexTypeSpec); ++i) {
    TypeSpecTableRow row;
    row.Signature = metadataReader.ReadBlobStreamIndex();
    metadata.GetTypeSpecTable().AddRow(std::move(row));
  }

  // Read ImplMap table 0x1C.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexImplMap); ++i) {
    ImplMapTableRow row;
    row.MappingFlags = static_cast<PInvokeAttributes>(metadataReader.Read<uint16_t>());
    // tables: Field, MethodDef
    row.MemberForwarded = metadataReader.ReadMemberForwardedCodedIndex();
    row.ImportName = metadataReader.ReadStringStreamIndex();
    row.ImportScope = metadataReader.ReadTableIndex(TableIndexModuleRef);
    metadata.GetImplMapTable().AddRow(std::move(row));
  }

  // Read FieldRVA table 0x1D.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexFieldRVA); ++i) {
    FieldRVATableRow row;
    row.RVA = metadataReader.Read<uint32_t>();
    row.Field = metadataReader.ReadTableIndex(TableIndexField);
    metadata.GetFieldRVATable().AddRow(std::move(row));
  }

  // Read Assembly table 0x20.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexAssembly); ++i) {
    AssemblyTableRow row;
    row.HashAlgId = metadataReader.Read<uint32_t>();
    row.MajorVersion = metadataReader.Read<uint16_t>();
    row.MinorVersion = metadataReader.Read<uint16_t>();
    row.BuildNumber = metadataReader.Read<uint16_t>();
    row.RevisionNumber = metadataReader.Read<uint16_t>();
    row.Flags = static_cast<AssemblyFlags>(metadataReader.Read<uint32_t>());
    row.PublicKey = metadataReader.ReadBlobStreamIndex();
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Culture = metadataReader.ReadStringStreamIndex();
    metadata.GetAssemblyTable().AddRow(std::move(row));

    const string &name = metadata.GetString(row.Name);
    printf("0x%x, %s\n", row.Name, name.c_str());
  }

  // Read AssemblyProcessor table 0x21.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexAssemblyProcessor); ++i) {
    AssemblyProcessorTableRow row;
    row.Processor = metadataReader.Read<uint32_t>();
    metadata.GetAssemblyProcessorTable().AddRow(std::move(row));
  }

  // Read AssemblyOS table 0x22.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexAssemblyOS); ++i) {
    AssemblyOSTableRow row;
    row.OSPlatformId = metadataReader.Read<uint32_t>();
    row.OSMajorVersion = metadataReader.Read<uint32_t>();
    row.OSMinorVersion = metadataReader.Read<uint32_t>();
    metadata.GetAssemblyOSTable().AddRow(std::move(row));
  }

  // Read AssemblyRef table 0x23.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexAssemblyRef); ++i) {
    AssemblyRefTableRow row;
    row.MajorVersion = metadataReader.Read<uint16_t>();
    row.MinorVersion = metadataReader.Read<uint16_t>();
    row.BuildNumber = metadataReader.Read<uint16_t>();
    row.ReversionNumber = metadataReader.Read<uint16_t>();
    row.Flags = static_cast<AssemblyFlags>(metadataReader.Read<uint32_t>());
    row.PublicKeyOrToken = metadataReader.ReadBlobStreamIndex();
    row.Name = metadataReader.ReadStringStreamIndex();
    row.Culture = metadataReader.ReadStringStreamIndex();
    row.HashValue = metadataReader.ReadBlobStreamIndex();
    metadata.GetAssemblyRefTable().AddRow(std::move(row));
  }

  // Read AssemblyRefProcessor table 0x24.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexAssemblyRefProcessor); ++i) {
    AssemblyRefProcessorTableRow row;
    row.Processor = metadataReader.Read<uint32_t>();
    row.AssemblyRef = metadataReader.ReadTableIndex(TableIndexAssemblyRef);
    metadata.GetAssemblyRefProcessorTable().AddRow(std::move(row));
  }

  // Read AssemblyRefOS table 0x25.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexAssemblyRefOS); ++i) {
    AssemblyRefOSTableRow row;
    row.OSPlatformId = metadataReader.Read<uint32_t>();
    row.OSMajorVersion = metadataReader.Read<uint32_t>();
    row.OSMinorVersion = metadataReader.Read<uint32_t>();
    row.AssemblyRef = metadataReader.ReadTableIndex(TableIndexAssemblyRef);
    metadata.GetAssemblyRefOSTable().AddRow(std::move(row));
  }

  // Read File table 0x26.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexFile); ++i) {
    FileTableRow row;
    row.Flags = static_cast<FileAttributes>(metadataReader.Read<uint32_t>());
    row.Name = metadataReader.ReadStringStreamIndex();
    row.HashValue = metadataReader.ReadBlobStreamIndex();
    metadata.GetFileTable().AddRow(std::move(row));
  }

  // Read ExportedType table 0x27.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexExportedType); ++i) {
    ExportedTypeTableRow row;
    row.Flags = static_cast<TypeAttributes>(metadataReader.Read<uint32_t>());
    row.TypeDefId = metadataReader.ReadTableIndex(TableIndexTypeDef);
    row.TypeName = metadataReader.ReadStringStreamIndex();
    row.TypeNamespace = metadataReader.ReadStringStreamIndex();
    // tables: File, ExportedType, AssemblyRef
    row.Implementation = metadataReader.ReadImplementationCodedIndex();
    metadata.GetExportedTypeTable().AddRow(std::move(row));
  }

  // Read ManifestResource table 0x28.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexManifestResource); ++i) {
    ManifestResourceTableRow row;
    row.Offset = metadataReader.Read<uint32_t>();
    row.Flags = static_cast<ManifestResourceAttributes>(metadataReader.Read<uint32_t>());
    row.Name = metadataReader.ReadStringStreamIndex();
    // conflict implementation. tables: File, AssemblyRef
    row.Implementation = metadataReader.ReadImplementationCodedIndex();
    metadata.GetManifestResourceTable().AddRow(std::move(row));
  }

  // Read NestedClass table 0x29.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexNestedClass); ++i) {
    NestedClassTableRow row;
    row.NestedClass = metadataReader.ReadTableIndex(TableIndexTypeDef);
    row.EnclosingClass = metadataReader.ReadTableIndex(TableIndexTypeDef);
    metadata.GetNestedClassTable().AddRow(std::move(row));
  }

  // Read GenericParam table 0x2A.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexGenericParam); ++i) {
    GenericParamTableRow row;
    row.Number = metadataReader.Read<uint16_t>();
    row.Flags = static_cast<GenericParamAttributes>(metadataReader.Read<uint16_t>());
    // tables: TypeDef, MethodDef
    row.Owner = metadataReader.ReadTypeOrMethodDefCodedIndex();
    row.Name = metadataReader.ReadStringStreamIndex();
    metadata.GetGenericParamTable().AddRow(std::move(row));
  }

  // Read MethodSpec table 0x2B.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexMethodSpec); ++i) {
    MethodSpecTableRow row;
    // tables: MethodDef, MemberRef
    row.Method = metadataReader.ReadMethodDefOrRefCodedIndex();
    row.Instantiation = metadataReader.ReadBlobStreamIndex();
    metadata.GetMethodSpecTable().AddRow(std::move(row));
  }

  // Read GenericParamConstraint table 0x2C.
  for (int i=0; i<metadata.GetTableRowsCount(TableIndexGenericParamConstraint); ++i) {
    GenericParamConstraintTableRow row;
    row.Owner = metadataReader.ReadTableIndex(TableIndexGenericParam);
    // tables: TableDef, TypeRef, TypeSpec
    row.Constraint = metadataReader.ReadTypeDefOrRefCodedIndex();
    metadata.GetGenericParamConstraintTable().AddRow(std::move(row));
  }
}
