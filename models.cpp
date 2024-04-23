//
// Created by admin on 2024/3/8.
//

#include "models.h"
#include "utils.h"
#include <filesystem>

namespace fs = std::filesystem;
using namespace Models;

#define NATURE_IDX(i) ((i)-1)

TypeDefinition::~TypeDefinition() {
//  for_each(_methods.begin(), _methods.end(), [](Method *method) {
//    delete method;
//  });
//  _methods.clear();
//  for_each(_fields.begin(), _fields.end(), [](Field *field) {
//    delete field;
//  });
//  _fields.clear();
}

Assembly::~Assembly() {
//  for_each(_typeDefinitions.begin(), _typeDefinitions.end(), [](TypeDefinition *typeDef) {
//    delete typeDef;
//  });
//  _typeDefinitions.clear();
}

ModelBuilder::ModelBuilder(const std::string &filePath): _filePath(filePath) {
  _workingDirectory = _filePath.parent_path();
}

// Shallow check type def/ref/spec is ValueType?
bool ModelBuilder::IsValueType(TypeDefOrRefCodedIndex codedIndex, const Metadata *metadata) {
  switch (codedIndex.GetTableIndex()) {
    case TableIndexTypeDef: {
      // Check typeName & typeNamespace
      const auto &row = metadata->GetTypeDefTable().GetRowAt(codedIndex.GetTableIndex());
      return strcmp(metadata->GetString(row.TypeNamespace), "System") == 0
             && strcmp(metadata->GetString(row.TypeName), "ValueType") == 0;
    }
    case TableIndexTypeRef: {
      // Check typeName & typeNamespace
      const auto &row = metadata->GetTypeRefTable().GetRowAt(codedIndex.GetRowIndex());
      return strcmp(metadata->GetString(row.TypeNamespace), "System") == 0
             && strcmp(metadata->GetString(row.TypeName), "ValueType") == 0;
    }
    case TableIndexTypeSpec: {
      // Read signature, check generic class is value type?
      BlobReader reader(metadata->GetBlobPointer<const uint8_t *>(codedIndex.GetRowIndex()));
      auto eleType = reader.Read<ELEMENT_TYPE>();
      if (eleType == ELEMENT_TYPE::GENERICINST) {
        auto superType = reader.Read<ELEMENT_TYPE>();
        return superType == ELEMENT_TYPE::VALUETYPE;
      }
      return false;
    }
    default:
      return false;
  }
}

void ModelBuilder::PrepareTypeDefinitionRef(AssemblyRef *assemblyRef,
                                            TableRowIndex i,
                                            const Metadata *metadata) {
  if (assemblyRef->_typeDefinitions[NATURE_IDX(i)] != nullptr)
    return;

  const TypeDefTableRow &typeDefRow = metadata->GetTypeDefTable().GetRowAt(i);
  auto typeRef = new TypeDefinitionRef();

  assemblyRef->_typeDefinitions[NATURE_IDX(i)] = typeRef;

  typeRef->_assembly = assemblyRef;
  // TODO fullName for refType.
  typeRef->_fullName = GetTypeRefFullName(assemblyRef->GetName(),
                                          metadata->GetString(typeDefRow.TypeNamespace),
                                          metadata->GetString(typeDefRow.TypeName));
  typeRef->_origFullName = typeRef->_origFullName;
  typeRef->_isValueType = IsValueType(typeDefRow.Extends, metadata);

  // Parse generic parameters.
  GetGenericParametersForType(i, typeRef->_genericContainer, metadata);

  // Declaring type.
  TableIndex declaringIndex = metadata->GetDeclaringType(i);
  if (declaringIndex != TableRowIndexInvalid) {
    PrepareTypeDefinitionRef(assemblyRef, declaringIndex, metadata);
    typeRef->_declaringType = assemblyRef->_typeDefinitions[NATURE_IDX(declaringIndex)];
  }
}

void ModelBuilder::PrepareAssemblyRef(TableRowIndex assemblyRefRowIndex, const Metadata *metadata) {
  const AssemblyTableRow &assemblyRow = metadata->GetAssemblyTable().GetRowAt(TableRowIndexStart);
  auto assemblyRef = new AssemblyRef();
  _assemblyRefs[NATURE_IDX(assemblyRefRowIndex)] = assemblyRef;

  assemblyRef->_name = metadata->GetString(assemblyRow.Name);
  //  printf("%s\n", assemblyRef->_name);

  assemblyRef->_typeDefinitions.resize(metadata->GetTypeDefTable().GetRowsCount());
  for (TableRowIndex typeDefIdx = TableRowIndexStart; typeDefIdx <= metadata->GetTypeDefTable().GetRowsCount(); ++typeDefIdx) {
    PrepareTypeDefinitionRef(assemblyRef, typeDefIdx, metadata);
  }
//  printf("Ref types, ready!");
}

void ModelBuilder::Build() {
  // Assembly <1 ... M> Module
  _mainDotNetFile = new DotNetFile(_filePath);
  _mainMetadata = MetadataParser::Parse(_mainDotNetFile->GetMetadataHeader());

  // Build assembly
  _mainAssembly = new Assembly();
  const AssemblyTableRow &assemblyRow = _mainMetadata->GetAssemblyTable().GetRowAt(TableRowIndexStart);
  _mainAssembly->_name = _mainMetadata->GetString(assemblyRow.Name);
  _mainAssembly->_culture = _mainMetadata->GetString(assemblyRow.Culture);
  _mainAssembly->_majorVersion = assemblyRow.MajorVersion;
  _mainAssembly->_minorVersion = assemblyRow.MinorVersion;
  _mainAssembly->_buildNumber = assemblyRow.BuildNumber;
  _mainAssembly->_reversionNumber = assemblyRow.RevisionNumber;

  // Build assembly refs.
  _assemblyRefs.resize(_mainMetadata->GetAssemblyRefTable().GetRowsCount());
  for (TableRowIndex assemblyRefRowIndex = TableRowIndexStart;
      assemblyRefRowIndex <= _mainMetadata->GetAssemblyRefTable().GetRowsCount(); ++assemblyRefRowIndex) {

    AssemblyRefTableRow row = _mainMetadata->GetAssemblyRefTable().GetRowAt(assemblyRefRowIndex);
    const char *assemblyName = _mainMetadata->GetString(row.Name);
    fs::path refFilePath = _workingDirectory / (std::string(assemblyName) + ".dll");

    printf("AssemblyRef: %d, %s\n", assemblyRefRowIndex, refFilePath.c_str());
    if (fs::exists(refFilePath)) {
      auto refDotNetFile = new DotNetFile(refFilePath);
      Metadata *refMetadata = MetadataParser::Parse(refDotNetFile->GetMetadataHeader());
      _refsDotNetFile.push_back(refDotNetFile);
      _refsMetadata.push_back(refMetadata);

      PrepareAssemblyRef(assemblyRefRowIndex, refMetadata);
    } else {
      assert(false && "Missing file");
    }
  }

  _typeDefinitions.resize(_mainMetadata->GetTableRowsCount(TableIndexTypeDef));
  _methods.resize(_mainMetadata->GetTableRowsCount(TableIndexMethodDef));

  // Build types def.
  for (TableRowIndex i=TableRowIndexStart; i<=_mainMetadata->GetTableRowsCount(TableIndexTypeDef); ++i) {
    const TypeDefinition *typeDef = GetTypeDefinition(i);
    _mainAssembly->_typeDefinitions.push_back(typeDef);
  }
}

std::string ModelBuilder::GetTypeDefinitionFullName(TableRowIndex typeDefRowIndex, bool orig) {
  const auto &typeDefRow = _mainMetadata->GetTypeDefTable().GetRowAt(typeDefRowIndex);
  std::string typeName = _mainMetadata->GetString(typeDefRow.TypeName);

  if (!orig) {
    auto dotPos = typeName.find('`');
    if (dotPos != std::string::npos)
      typeName = typeName.substr(0, dotPos);
  }

  std::string typeNamespace = _mainMetadata->GetString(typeDefRow.TypeNamespace);
  auto enclosingTypeDefRowIndex = _mainMetadata->GetEnclosingType(typeDefRowIndex);
  if (enclosingTypeDefRowIndex != TableRowIndexInvalid)
    return GetTypeDefinitionFullName(enclosingTypeDefRowIndex, false) + "." + typeName;

  return typeNamespace.empty() ? typeName : typeNamespace + "." + typeName;
}

const TypeDefinition *ModelBuilder::GetTypeDefinition(TableRowIndex typeDefRowIndex) {
  if (_typeDefinitions.at(NATURE_IDX(typeDefRowIndex)) != nullptr)
    return _typeDefinitions.at(NATURE_IDX(typeDefRowIndex));

  auto &typeDefRow = _mainMetadata->GetTypeDefTable().GetRowAt(typeDefRowIndex);
  auto typeDef = new TypeDefinition();
  // Cache it, prevent recursion.
  _typeDefinitions[NATURE_IDX(typeDefRowIndex)] = typeDef;

  typeDef->_rowIndex = typeDefRowIndex;
  typeDef->_typeIndex = GetTypeDefTypeIndex(NATURE_IDX(typeDefRowIndex));
  std::string fullNamePrefix = _mainAssembly->GetName();
  Utils::StringReplace(fullNamePrefix, ".", "_");
  fullNamePrefix += "_dll.";
  typeDef->_fullName = fullNamePrefix + GetTypeDefinitionFullName(typeDefRowIndex);
  typeDef->_origFullName = fullNamePrefix + GetTypeDefinitionFullName(typeDefRowIndex, true);
  typeDef->_isInterface = Utils::BitTest(typeDefRow.Flags, TypeAttributes::Interface);
  typeDef->_isAbstract = Utils::BitTest(typeDefRow.Flags, TypeAttributes::Abstract);
  typeDef->_isSealed = Utils::BitTest(typeDefRow.Flags, TypeAttributes::Sealed);

  // Parse generic(generic container).
  GetGenericParametersForType(typeDefRowIndex, typeDef->_genericContainer, _mainMetadata);
  GenericContext classGenericCtx = {
    .classContainer = &typeDef->_genericContainer
  };

  // Parse nested class.
  TableIndex declaringIndex = _mainMetadata->GetDeclaringType(typeDefRowIndex);
  if (declaringIndex != TableRowIndexInvalid) {
    typeDef->_declaringType = GetTypeDefinition(declaringIndex);
  }

  // Parent.
  if (typeDefRow.Extends.GetRowIndex() != TableRowIndexInvalid) {
    auto parentType = GetType(typeDefRow.Extends, &classGenericCtx, true);
    typeDef->_parent = parentType;

    // Check parent is Enum or ValueType?
    if (TypeType::Class == parentType->GetType()) {
      auto parentClassType = reinterpret_cast<ClassType *>(parentType);

      auto parentOrigFullName = parentClassType->GetTypeDef()->GetOrigFullName();

      typeDef->_isEnum = parentOrigFullName == "mscorlib_dll.System.Enum";
      typeDef->_isValueType = typeDef->_isEnum || parentOrigFullName == "mscorlib_dll.System.ValueType";
    }
  }

  // Parse interfaces.
  const auto &interfaceIndexes = _mainMetadata->GetInterfaceImplsForType(typeDefRowIndex);
  if (!interfaceIndexes.empty()) {
//    printf("Type: 0x%x, %s.%s\n", typeDef->_index, typeDef->_typeNamespace, typeDef->_typeName);
    for (TypeDefOrRefCodedIndex itfIndex : interfaceIndexes) {
      const IType *itfType = GetType(itfIndex, &classGenericCtx, true);
      typeDef->_interfaces.push_back(itfType);
    }
  }

  // Parse fields & methods.
  bool isLastTypeDefIndex = typeDefRowIndex == _mainMetadata->GetTableRowsCount(TableIndexTypeDef);
  TableRowIndex fieldRowStartIndex = typeDefRow.FieldList;
  TableRowIndex methodRowStartIndex = typeDefRow.MethodList;

  uint32_t fieldsCount, methodsCount;
  if (isLastTypeDefIndex) {
    fieldsCount = _mainMetadata->GetTableRowsCount(TableIndexField) + 1 - fieldRowStartIndex;
    methodsCount = _mainMetadata->GetTableRowsCount(TableIndexMethodDef) + 1 - methodRowStartIndex;
  } else {
    fieldsCount = _mainMetadata->GetTypeDefTable().GetRowAt(typeDefRowIndex + 1).FieldList - fieldRowStartIndex;
    methodsCount = _mainMetadata->GetTypeDefTable().GetRowAt(typeDefRowIndex + 1).MethodList - methodRowStartIndex;
  }

  for (TableRowIndex fieldRowIndex = fieldRowStartIndex; fieldRowIndex < fieldRowStartIndex + fieldsCount; ++fieldRowIndex) {
    typeDef->_fields.push_back(GetField(typeDef, fieldRowIndex));
  }

  for (TableRowIndex methodRowIndex = methodRowStartIndex; methodRowIndex < methodRowStartIndex + methodsCount; ++methodRowIndex) {
    typeDef->_methods.push_back(GetMethod(typeDef, methodRowIndex));
  }

//  printf("%s, fields: %d, methods: %d\n", typeDef->_name, fieldsCount, methodsCount);
//  printf("TypeName: %s, namespace: %s, isGeneric: %d\n", typeDef->_name, typeDef->_namespace, typeDef->_isGeneric);

  return typeDef;
}

Field *ModelBuilder::GetField(Models::TypeDefinition *typeDef, TableRowIndex fieldRowIndex) {
  const auto &fieldRow = _mainMetadata->GetFieldTable().GetRowAt(fieldRowIndex);

  BlobReader blobReader = GetBlobReader(fieldRow.Signature);
  auto abbr = blobReader.Read<SignatureAbbreviation>();
  assert(Utils::BitTest(abbr, SignatureAbbreviation::FIELD) && "Not field signature.");

  GenericContext classGenericCtx = {
    .classContainer = &typeDef->_genericContainer
  };

  auto field = new Field();
  field->_type = GetType(blobReader, &classGenericCtx, false);
  field->_rowIndex = fieldRowIndex;
  field->_isLiteral = Utils::BitTest(fieldRow.Flags, FieldAttributes::Literal);
  field->_isInitOnly = Utils::BitTest(fieldRow.Flags, FieldAttributes::InitOnly);
  field->_isStatic = field->_isLiteral || Utils::BitTest(fieldRow.Flags, FieldAttributes::Static);
  field->_name = _mainMetadata->GetString(fieldRow.Name);
//    printf("Type: %s, Field: Index: 0x%x, Name: %s, Signature: %08x\n", typeDef->_typeName, fieldRowIndex, field->_name, fieldRow.Signature);

  return field;
}

Method *ModelBuilder::GetMethod(Models::TypeDefinition *typeDef, TableRowIndex methodRowIndex) {
  const auto &methodRow = _mainMetadata->GetMethodDefTable().GetRowAt(methodRowIndex);
  auto method = new Method();
  typeDef->_methods.push_back(method);
  _methods[NATURE_IDX(methodRowIndex)] = method;

  method->_typeDefinition = typeDef;

  method->_encodedMethodIndex = GetMethodDefEncodedIndex(NATURE_IDX(methodRowIndex));
  method->_name = _mainMetadata->GetString(methodRow.Name);

  method->_isStatic = Utils::BitTest(methodRow.Flags, MethodAttributes::Static);
  method->_isFinal = Utils::BitTest(methodRow.Flags, MethodAttributes::Final);
  method->_isVirtual = Utils::BitTest(methodRow.Flags, MethodAttributes::Virtual);
  method->_isAbstract = Utils::BitTest(methodRow.Flags, MethodAttributes::Abstract);
  method->_isGeneric = typeDef->GetIsGeneric();

  // Get generic parameters if needed.
  GetGenericParametersForMethod(methodRowIndex, method->_genericContainer, _mainMetadata);

  // II.23.2.1 MethodDefSig
  BlobReader methodSigReader = GetBlobReader(methodRow.Signature);
  auto abbr = methodSigReader.Read<SignatureAbbreviation>();
  bool hasThis = false;
  bool explicitThis = false;
  bool dft = false;
  bool varArg = false;
  bool generic = false;
  uint32_t genParamCount = 0;

  // The first byte of the Signature holds bits for HASTHIS, EXPLICITTHIS and calling convention (DEFAULT, VARARG, or GENERIC). These are ORed together.
  if (Utils::BitTest(abbr, SignatureAbbreviation::HASTHIS)) {
    hasThis = true;
    if (Utils::BitTest(abbr, SignatureAbbreviation::EXPLICITTHIS)) {
      explicitThis = true;
    }
  }

  if (Utils::BitTest(abbr, SignatureAbbreviation::DEFAULT)) {
    dft = true;
  } else if (Utils::BitTest(abbr, SignatureAbbreviation::VARARG)) {
    varArg = true;
  } else if (Utils::BitTest(abbr, SignatureAbbreviation::GENERIC)) {
    generic = true;
    genParamCount = methodSigReader.ReadCompressedUInt32();
  }

  uint32_t sigParamCount = methodSigReader.ReadCompressedUInt32();

//    printf("Method: 0x%x, %s, hasThis: %d, explicitThis: %d, default: %d, varArg: %d, generic: %d, genParamCount: %d, paramCount: %d\n",
//           methodRowIndex, method->_name, hasThis, explicitThis, dft, varArg, generic, genParamCount, sigParamCount);
  // II.23.2.11 RetType
  GenericContext methodGenericCtx = {
    .classContainer = &typeDef->_genericContainer,
    .methodContainer = &method->_genericContainer
  };

  IType *retType = GetType(methodSigReader, &methodGenericCtx, false);
  method->_returnType = retType;

  // Params
  TableRowIndex paramRowStartIndex = methodRow.ParamList;
  uint32_t paramsCount = 0;
  bool isLastMethod = methodRowIndex == _mainMetadata->GetMethodDefTable().GetRowsCount();
  if (isLastMethod) {
    paramsCount = _mainMetadata->GetTableRowsCount(TableIndexParam) + 1 - paramRowStartIndex;
  } else {
    paramsCount = _mainMetadata->GetMethodDefTable().GetRowAt(methodRowIndex + 1).ParamList - paramRowStartIndex;
  }

//    assert(sigParamCount == paramsCount && "Param count mismatched!");

  for (auto paramRowIndex = paramRowStartIndex; paramRowIndex < paramRowStartIndex + paramsCount; ++ paramRowIndex) {
    ParamTableRow paramRow = _mainMetadata->GetParamTable().GetRowAt(paramRowIndex);
    Parameter param {};
    param._type = GetType(methodSigReader, &methodGenericCtx, false);

    param._name = _mainMetadata->GetString(paramRow.Name);
    param._in = Utils::BitTest(paramRow.Flags, ParamAttributes::In);
    param._out = Utils::BitTest(paramRow.Flags, ParamAttributes::Out);
    param._optional = Utils::BitTest(paramRow.Flags, ParamAttributes::Optional);

    // Prepare to fetch next.
    paramRowIndex ++;

    method->_parameters.push_back(param);
  }

  return method;
}

const ITypeDefinition *ModelBuilder::GetTypeDefinitionFromTypeRef(TableRowIndex typeRefRowIndex, bool decay) {
  const TypeRefTableRow &row = _mainMetadata->GetTypeRefTable().GetRowAt(typeRefRowIndex);
  const char *typeName = _mainMetadata->GetString(row.TypeName);
  const char *typeNamespace = _mainMetadata->GetString(row.TypeNamespace);
//  printf("GetFromTypeRef, %d, %s, %s\n", row.ResolutionScope.GetTableIndex(), typeNamespace, typeName);

  switch (row.ResolutionScope.GetTableIndex()) {
    case TableIndexAssemblyRef: {
//      printf("** AssemblyRef\n");
      auto assemblyRef = _assemblyRefs[NATURE_IDX(row.ResolutionScope.GetRowIndex())];
      auto typeRef = GetTypeDefinitionRef(assemblyRef, nullptr, typeNamespace, typeName);
      assert(typeRef && "Unknown type ref");
      return typeRef;
    }
    case TableIndexTypeRef: {
//      printf("** TypeRef\n");
      auto enclosingType = reinterpret_cast<ClassType *>(GetTypeFromTypeRef(row.ResolutionScope.GetRowIndex(), decay));
      auto assemblyRef = reinterpret_cast<const AssemblyRef *>(enclosingType->GetTypeDef()->GetAssembly());
      auto typeRef = GetTypeDefinitionRef(assemblyRef, enclosingType->GetTypeDef(), typeNamespace, typeName);
      assert(typeRef && "Unknown type ref");
      return typeRef;
    }
    default:
      assert(false && "Unsupported type ref resolution scope.");
  }
}

const ITypeDefinition *ModelBuilder::GetTypeDefinitionFromTypeSpec(TableRowIndex typeSpecRowIndex, const GenericContext *context, bool decay) {
  auto classType = reinterpret_cast<ClassType *>(GetTypeFromTypeSpec(typeSpecRowIndex, context, decay));
  return classType->GetTypeDef();
}

std::string ModelBuilder::GetTypeRefFullName(const std::string &assemblyName, const std::string &typeNamespace,
                                             const std::string &typeName) {
  std::string fullNamePrefix = assemblyName;
  Utils::StringReplace(fullNamePrefix, ".", "_");
  fullNamePrefix += "_dll";
  std::string fullName = fullNamePrefix;

  if (!typeNamespace.empty())
    fullName += "." + typeNamespace;

  fullName += typeName;
  return fullName;
}

const TypeDefinitionRef *ModelBuilder::GetTypeDefinitionRef(const Models::AssemblyRef *assemblyRef,
                                                            const Models::ITypeDefinition *declaringType,
                                                            const char *typeNamespace,
                                                            const char *typeName) {

  auto typeRefFullName = GetTypeRefFullName(assemblyRef->GetName(), typeNamespace, typeName);

  for (const auto *typeRef : assemblyRef->_typeDefinitions) {
    if (declaringType != nullptr && declaringType != typeRef->_declaringType)
      continue;

    if (typeRef->GetOrigFullName() == typeRefFullName)
      return typeRef;
  }

  return nullptr;
}

void ModelBuilder::GetGenericParametersForType(TableRowIndex typeDefRowIndex, GenericContainer &container, const Metadata *metadata) {
  const std::vector<TableRowIndex> &genericParamRowIndexes = metadata->GetGenericParamsForType(typeDefRowIndex);
  if (!genericParamRowIndexes.empty()) {
    container.resize(genericParamRowIndexes.size());
    for (TableRowIndex genericParamRowIndex : genericParamRowIndexes) {
      const auto &genericParamRow = metadata->GetGenericParamTable().GetRowAt(genericParamRowIndex);
      container[genericParamRow.Number] = metadata->GetString(genericParamRow.Name);
    }
  }
}

void ModelBuilder::GetGenericParametersForMethod(TableRowIndex methodDefRowIndex, GenericContainer &container, const Metadata *metadata) {
  const std::vector<TableRowIndex> &genericParamRowIndexes = metadata->GetGenericParamsForMethod(methodDefRowIndex);
  if (!genericParamRowIndexes.empty()) {
    container.resize(genericParamRowIndexes.size());
    for (TableRowIndex genericParamRowIndex : genericParamRowIndexes) {
      const auto &genericParamRow = metadata->GetGenericParamTable().GetRowAt(genericParamRowIndex);
      container[genericParamRow.Number] = metadata->GetString(genericParamRow.Name);
    }
  }
}

IType *ModelBuilder::GetType(BlobReader &reader, const GenericContext *context, bool decay) {
  // II.23.2.12 Type
  // Type is encoded in signatures as follows (I1 is an abbreviation for
  // ELEMENT_TYPE_I1, U1 is an abbreviation for ELEMENT_TYPE_U1, and so on;
  // see II.23.1.16):
  auto elementType = reader.Read<ELEMENT_TYPE>();
  switch (elementType) {
    // BOOLEAN | CHAR | I1 | U1 | I2 | U2 | I4 | U4 | I8 | U8 | R4 | R8 | I | U
    case ELEMENT_TYPE::VOID:
      return _voidType;
    case ELEMENT_TYPE::BOOLEAN:
      return _booleanType;
    case ELEMENT_TYPE::CHAR:
      return _charType;
    case ELEMENT_TYPE::I1:
      return _i1Type;
    case ELEMENT_TYPE::U1:
      return _u1Type;
    case ELEMENT_TYPE::I2:
      return _i2Type;
    case ELEMENT_TYPE::U2:
      return _u2Type;
    case ELEMENT_TYPE::I4:
      return _i4Type;
    case ELEMENT_TYPE::U4:
      return _u4Type;
    case ELEMENT_TYPE::I8:
      return _i8Type;
    case ELEMENT_TYPE::U8:
      return _u8Type;
    case ELEMENT_TYPE::R4:
      return _r4Type;
    case ELEMENT_TYPE::R8:
      return _r8Type;
    case ELEMENT_TYPE::I:
      return _iType;
    case ELEMENT_TYPE::U:
      return _uType;
    case ELEMENT_TYPE::STRING: {
      // | STRING
      if (decay)
        return _stringType;
      return new PointerType(_stringType);
    }
    case ELEMENT_TYPE::OBJECT: {
      // | OBJECT
      if (decay)
        return _objectType;
      return new PointerType(_objectType);
    }
    case ELEMENT_TYPE::TYPEDBYREF: {
      // | TYPEDBYREF
      if (decay)
        return _typedByRefType;
      return new PointerType(_typedByRefType);
    }
    case ELEMENT_TYPE::VALUETYPE: {
      // | VALUETYPE TypeDefOrRefOrSpecEncoded
      TypeDefOrRefOrSpecEncoded encoded(reader.ReadCompressedUInt32());
      return GetType(encoded, context, decay);
    }
    case ELEMENT_TYPE::CLASS: {
      // | CLASS TypeDefOrRefOrSpecEncoded
      TypeDefOrRefOrSpecEncoded encoded(reader.ReadCompressedUInt32());
      return GetType(encoded, context, decay);
    }
    case ELEMENT_TYPE::GENERICINST: {
      // | GENERICINST (CLASS | VALUETYPE) TypeDefOrRefOrSpecEncoded GenArgCount Type *
      const ClassType *genericClass = reinterpret_cast<ClassType *>(GetType(reader, nullptr, true));
      const GenericContainer &selfGenericContainer = genericClass->GetTypeDef()->GetGenericContainer();
      uint32_t genArgCount = reader.ReadCompressedUInt32();

      assert(genArgCount == selfGenericContainer.size() && "Inflated argc does not matched.");

      auto genericInstanceType = new GenericInstanceType(genericClass->GetTypeDef());
      for (int i = 0; i < genArgCount; ++i) {
        InflatedVariable inflatedVariable {};
        inflatedVariable._isMethod = false;
        inflatedVariable._name = selfGenericContainer.at(i);
        inflatedVariable._type = GetType(reader, context, false);
        genericInstanceType->_inflatedVariables.push_back(inflatedVariable);
      }
      if (decay)
        return genericInstanceType;
      return new PointerType(genericInstanceType);
    }
//    case ELEMENT_TYPE::FNPTR: {
//      // | FNPTR MethodDefSig
//      // | FNPTR MethodRefSig
//    }
    case ELEMENT_TYPE::VAR: {
      // | VAR number
      uint32_t number = reader.ReadCompressedUInt32();
      assert(context && context->methodContainer && "Class container is null");
      return new GenericParameterType(context->classContainer->at(number));
    }
    case ELEMENT_TYPE::MVAR: {
      // | MVAR number
      uint32_t number = reader.ReadCompressedUInt32();
      assert(context && context->methodContainer && "Method container is null");
      return new GenericParameterType(context->methodContainer->at(number));
    }
    case ELEMENT_TYPE::SZARRAY: {
      // | SZARRAY CustomMod* Type (single dimensional, zero-based array i.e., vector)
      auto arrEleType = GetType(reader, context, false);
      auto arrType = new ArrayType(arrEleType);
      if (decay)
        return arrType;
      return new PointerType(arrType);
    }
    case ELEMENT_TYPE::ARRAY: {
      // ARRAY Type ArrayShape (general array, see §II.23.2.13)
      IType *arrEleType = GetType(reader, context, false);
      uint32_t rank = reader.ReadCompressedUInt32();
      uint32_t numSizes = reader.ReadCompressedUInt32();
      for (int i = 0; i < numSizes; ++i) {
        uint32_t size = reader.ReadCompressedUInt32();
      }
      uint32_t numLowBounds = reader.ReadCompressedUInt32();
      for (int i = 0; i < numLowBounds; ++i) {
        uint32_t lowBound = reader.ReadCompressedUInt32();
      }
      auto arrType = new ArrayType(arrEleType);
      if (decay)
        return arrType;
      return new PointerType(arrType);
    }
    case ELEMENT_TYPE::BYREF: {
      IType *pointeeType = GetType(reader, context, false);
      return new PointerType(pointeeType);
    }
    case ELEMENT_TYPE::CMOD_OPT: {
      uint32_t encodedToken = reader.ReadCompressedUInt32();
      return GetType(reader, context, false);
    }
    default:
      assert(false && "Unsupported element type");
  }
}

IType *ModelBuilder::GetType(const TypeDefOrRefCodedIndex &codedIndex, const GenericContext *context, bool decay) {
  switch (codedIndex.GetTableIndex()) {
    case TableIndexTypeDef:
      return GetTypeFromTypeDef(codedIndex.GetRowIndex(), decay);
    case TableIndexTypeRef:
      return GetTypeFromTypeRef(codedIndex.GetRowIndex(), decay);
    case TableIndexTypeSpec:
      return GetTypeFromTypeSpec(codedIndex.GetRowIndex(), context, decay);
    default:
      assert(false && "Unsupported table type");
  }
}

IType *ModelBuilder::GetTypeFromTypeRef(TableRowIndex typeRefRowIndex, bool decay) {
  const ITypeDefinition *typeRef = GetTypeDefinitionFromTypeRef(typeRefRowIndex, decay);
  auto classType = new ClassType(typeRef);

  if (decay || typeRef->GetIsValueType()) {
    return classType;
  } else {
    return new PointerType(classType);
  }
}

IType *ModelBuilder::GetTypeFromTypeDef(TableRowIndex typeDefRowIndex, bool decay) {
  const TypeDefinition *typeDef = GetTypeDefinition(typeDefRowIndex);
  auto classType = new ClassType(typeDef);

  if (decay || typeDef->GetIsValueType()) {
    return classType;
  } else {
    return new PointerType(classType);
  }
}

IType *ModelBuilder::GetTypeFromTypeSpec(TableRowIndex typeSpecRowIndex, const GenericContext *context, bool decay) {
  // II.23.2.14 TypeSpec
  // The signature in the Blob heap indexed by a TypeSpec token has the following format –
  // TypeSpecBlob ::=
  //    PTR           CustomMod* VOID
  //  | PTR           CustomMod* Type
  //  | FNPTR         MethodDefSig
  //  | FNPTR         MethodRefSig
  //  | ARRAY         Type ArrayShape
  //  | SZARRAY       CustomMod* Type
  //  | GENERICINST   (CLASS | VALUETYPE) TypeDefOrRefOrSpecEncoded GenArgCount Type Type*
  // For compactness, the ELEMENT_TYPE_ prefixes have been omitted from this list.
  // So, for example, “PTR” is shorthand for ELEMENT_TYPE_PTR. (§II.23.1.16) Note that a TypeSpecBlob
  // does not begin with a calling-convention byte, so it differs from the various other signatures that
  // are stored into Metadata.

  const auto &typeSpecRow = _mainMetadata->GetTypeSpecTable().GetRowAt(typeSpecRowIndex);
  BlobReader typeSpecReader = GetBlobReader(typeSpecRow.Signature);
  return GetType(typeSpecReader, context, decay);
}

IType *ModelBuilder::GetType(const TypeDefOrRefOrSpecEncoded &encoded, const GenericContext *context, bool decay) {
  TableRowIndex rowIndex = encoded.GetRowIndex();
  switch (encoded.GetTableIndex()) {
    case TableIndexTypeDef:
      return GetTypeFromTypeDef(rowIndex, decay);
    case TableIndexTypeRef:
      return GetTypeFromTypeRef(rowIndex, decay);
    case TableIndexTypeSpec:
      return GetTypeFromTypeSpec(rowIndex, context, decay);
    default:
      assert(false && "Unsupported type");
  }
}

BlobReader ModelBuilder::GetBlobReader(BlobIndex index) {
//  // II.24.2.4 #US and #Blob heaps
  BlobReader reader(_mainMetadata->GetBlobPointer<const uint8_t *>(index));
  return reader;
}

int ModelBuilder::GetTypeDefTypeIndex(int typeDefIndex) {
  return static_cast<int>(EncodeHybridClrIndex(static_cast<uint>(typeDefIndex)));
}

uint ModelBuilder::GetMethodDefEncodedIndex(int methodDefIndex) {
  return EncodeIndex(3, EncodeHybridClrIndex(static_cast<uint>(methodDefIndex)));
}

uint ModelBuilder::EncodeHybridClrIndex(uint index) {
  return 0x4000000 | index;
}

uint ModelBuilder::EncodeIndex(uint usage, uint index) {
  return (usage << 29) | (index << 1);
}