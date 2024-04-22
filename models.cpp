//
// Created by admin on 2024/3/8.
//

#include "models.h"
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;
using namespace Models;

#define NATURE_IDX(i) ((i)-1)
template<class T>

static bool BIT_TEST(T a, T b) {
  return (static_cast<std::underlying_type_t<T>>(a) & static_cast<std::underlying_type_t<T>>(b)) != 0;
}

template<class T>
static T BIT_MASK(T a, T b) {
  auto v = static_cast<std::underlying_type_t<T>>(a) & static_cast<std::underlying_type_t<T>>(b);
  return static_cast<T>(v);
}

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
  auto typeDefRef = new TypeDefinitionRef();

  assemblyRef->_typeDefinitions[NATURE_IDX(i)] = typeDefRef;

  typeDefRef->_assembly = assemblyRef;
  typeDefRef->_typeName = metadata->GetString(typeDefRow.TypeName);
  typeDefRef->_typeNamespace = metadata->GetString(typeDefRow.TypeNamespace);
  typeDefRef->_isValueType = IsValueType(typeDefRow.Extends, metadata);
  // Parse generic parameters.
  GetGenericParametersForType(i, typeDefRef->_genericContainer, metadata);

  // Declaring type.
  TableIndex declaringIndex = metadata->GetDeclaringType(i);
  if (declaringIndex != TableIndexInvalid) {
    PrepareTypeDefinitionRef(assemblyRef, declaringIndex, metadata);
    typeDefRef->_declaringType = assemblyRef->_typeDefinitions[NATURE_IDX(declaringIndex)];
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
  Assembly assembly;
  const AssemblyTableRow &assemblyRow = _mainMetadata->GetAssemblyTable().GetRowAt(TableRowIndexStart);
  assembly._name = _mainMetadata->GetString(assemblyRow.Name);
  assembly._culture = _mainMetadata->GetString(assemblyRow.Culture);
  assembly._majorVersion = assemblyRow.MajorVersion;
  assembly._minorVersion = assemblyRow.MinorVersion;
  assembly._buildNumber = assemblyRow.BuildNumber;
  assembly._reversionNumber = assemblyRow.RevisionNumber;

  // Build assembly refs.
  _assemblyRefs.resize(_mainMetadata->GetAssemblyRefTable().GetRowsCount());
  for (TableRowIndex assemblyRefRowIndex = TableRowIndexStart;
      assemblyRefRowIndex <= _mainMetadata->GetAssemblyRefTable().GetRowsCount(); ++assemblyRefRowIndex) {

    AssemblyRefTableRow row = _mainMetadata->GetAssemblyRefTable().GetRowAt(assemblyRefRowIndex);
    const char *assemblyName = _mainMetadata->GetString(row.Name);
    fs::path refFilePath = _workingDirectory / (string(assemblyName) + ".dll");

//    printf("AssemblyRef: %d, %s\n", assemblyRefRowIndex, refFilePath.c_str());
    if (fs::exists(refFilePath)) {
      auto refDotNetFile = new DotNetFile(refFilePath);
      Metadata *refMetadata = MetadataParser::Parse(refDotNetFile->GetMetadataHeader());
      _refsDotNetFile.push_back(refDotNetFile);
      _refsMetadata.push_back(refMetadata);

      PrepareAssemblyRef(assemblyRefRowIndex, refMetadata);
    } else {
      throw "Missing file";
    }
  }

//  exit(0);
  _typeDefinitions.resize(_mainMetadata->GetTableRowsCount(TableIndexTypeDef));
  _methods.resize(_mainMetadata->GetTableRowsCount(TableIndexMethodDef));

  // Build types def.

  for (TableRowIndex i=TableRowIndexStart; i<=_mainMetadata->GetTableRowsCount(TableIndexTypeDef); ++i) {
    const TypeDefinition *typeDef = GetTypeDefinitionFromTypeDef(i);
    assembly._typeDefinitions.push_back(typeDef);
  }
//  for (TableRowIndex i=TableRowIndexStart; i<=_mainMetadata->GetTableRowsCount(TableIndexMethodSpec); ++i) {
//    const auto &methodSpecRow = _mainMetadata->GetMethodSpecTable().GetRowAt(i);
//    switch (methodSpecRow.Method.GetTableIndex()) {
//      case TableIndexMethodDef:
//        printf("MethodDef: 0x%x\n", methodSpecRow.Method.GetRowIndex());
//        break;
//      case TableIndexMemberRef: {
//        const auto &memberRefRow = _mainMetadata->GetMemberRefTable().GetRowAt(methodSpecRow.Method.GetRowIndex());
//        switch (memberRefRow.Class.GetTableIndex()) {
//          // MethodDef, ModuleRef,TypeDef, TypeRef, or TypeSpec
//          case TableIndexMethodDef:
//            printf("MemberRef - MethodDef: 0x%x, Name: %s\n", memberRefRow.Class.GetRowIndex(), _mainMetadata->GetString(memberRefRow.Name));
//            break;
//          case TableIndexModuleRef:
//            printf("MemberRef - ModuleRef: 0x%x, Name: %s\n", memberRefRow.Class.GetRowIndex(), _mainMetadata->GetString(memberRefRow.Name));
//            break;
//          case TableIndexTypeDef:
//            printf("MemberRef - TypeDef: 0x%x, Name: %s\n", memberRefRow.Class.GetRowIndex(), _mainMetadata->GetString(memberRefRow.Name));
//            break;
//          case TableIndexTypeRef:
//            printf("MemberRef - TypeRef: 0x%x, Name: %s\n", memberRefRow.Class.GetRowIndex(), _mainMetadata->GetString(memberRefRow.Name));
//            break;
//          case TableIndexTypeSpec:
//            printf("MemberRef - TypeSpec: 0x%x, Name: %s\n", memberRefRow.Class.GetRowIndex(), _mainMetadata->GetString(memberRefRow.Name));
//            break;
//        }
//        break;
//      }
//
//    }
//  }


  for (TableRowIndex i=TableRowIndexStart; i<=_mainMetadata->GetTableRowsCount(TableIndexMethodSpec); ++i) {
    const auto &methodSpecRow = _mainMetadata->GetMethodSpecTable().GetRowAt(i);
    TableIndex tableIndex = methodSpecRow.Method.GetTableIndex();
    TableRowIndex rowIndex = methodSpecRow.Method.GetRowIndex();
    switch (tableIndex) {
      case TableIndexMethodDef: {
        BlobReader methodSpecSigReader = GetBlobReader(methodSpecRow.Instantiation);
        uint8_t _1stByte = methodSpecSigReader.Read<uint8_t>();
        assert(_1stByte == 0x0A && "Invalid method spec signature!");
        uint32_t genArgCount = methodSpecSigReader.ReadCompressedUInt32();
        const Method *method = _methods[NATURE_IDX(methodSpecRow.Method.GetRowIndex())];
//        printf("[MethodDef] TypeDef: %s.%s, Method: %s, classGC: %d, methodGC: %d\n",
//               method->_typeDefinition->GetTypeNamespace(),
//               method->_typeDefinition->GetTypeName(),
//               method->_name,
//               method->_typeDefinition->_genericContainer.size(),
//               method->_genericContainer.size());
//        GenericContext context = {
//          .classContainer = &method->_typeDefinition->_genericContainer,
//          .methodContainer = &method->_genericContainer
//        };
//
//        for (int j = 0; j < genArgCount; ++j) {
//          // skip has any MVAR specs.
//          ELEMENT_TYPE eleType = *reinterpret_cast<const ELEMENT_TYPE *>(methodSpecSigReader.GetData());
////          if (eleType == ELEMENT_TYPE::VAR || eleType == ELEMENT_TYPE::MVAR)
////            continue;
////          IType *argType = GetTypeFromBlob(methodSpecSigReader, context);
////          printf("1234\n");
//        }

        break;
      }
      case TableIndexMemberRef: {
        const auto &memberRefRow = _mainMetadata->GetMemberRefTable().GetRowAt(rowIndex);
        const char *name = _mainMetadata->GetString(memberRefRow.Name);

        switch (memberRefRow.Class.GetTableIndex()) {
          case TableIndexMethodDef: {
            auto method = _methods[NATURE_IDX(memberRefRow.Class.GetRowIndex())];
            printf("[MemberRef] %s -> MethodDef: TypeIndex: 0x%x, TypeName: %s.%s, MethodIndex: 0x%x, MethodName: %s\n",
                   name,
                   method->_typeDefinition->_index,
                   method->_typeDefinition->_typeNamespace,
                   method->_typeDefinition->_typeName,
                   method->_index,
                   method->_name);
            break;
          }
          case TableIndexModuleRef: {
            printf("[MemberRef] %s -> ModuleRef\n", name);
            break;
          }
          case TableIndexTypeDef: {
            auto typeDef = _typeDefinitions[NATURE_IDX(rowIndex)];
            printf("[MemberRef] %s -> TypeDef: TypeIndex: 0x%x, TypeName: %s.%s\n",
                   name,
                   typeDef->_index,
                   typeDef->_typeNamespace,
                   typeDef->_typeName);
            break;
          }
          case TableIndexTypeRef: {
            break;
          }
          case TableIndexTypeSpec: {
            printf("[MemberRef] %s -> TypeSpec\n", name);

            break;
          }
          default:
            break;
        }

        break;
      }
      default:
        // We don't care about any specs from outer ref.
        break;
    }
  }

  for (TableRowIndex i=TableRowIndexStart; i<=_mainMetadata->GetTableRowsCount(TableIndexMemberRef); ++i) {
    const auto &memberRefRow = _mainMetadata->GetMemberRefTable().GetRowAt(i);
    TableRowIndex rowIndex = memberRefRow.Class.GetRowIndex();
    const char *name = _mainMetadata->GetString(memberRefRow.Name);
    if (strcmp("get_Instance", name))
      continue;

    printf("MemberRef: Name: %s\n", name);
    switch (memberRefRow.Class.GetTableIndex()) {
      case TableIndexMethodDef: {
        auto method = _methods[NATURE_IDX(rowIndex)];

//        if (method->GetIsGeneric() || method->_typeDefinition->GetIsGeneric()) {
          printf("  -> MethodDef: TypeIndex: 0x%x, TypeName: %s.%s, MethodIndex: 0x%x, MethodName: %s\n",
                 method->_typeDefinition->_index,
                 method->_typeDefinition->_typeNamespace,
                 method->_typeDefinition->_typeName,
                 method->_index,
                 method->_name);
//        }

        break;
      }
      case TableIndexModuleRef: {
        printf("  -> ModuleRef\n");
        break;
      }
      case TableIndexTypeDef: {
//        auto typeDef = _typeDefinitions[NATURE_IDX(rowIndex)];
//        printf("  -> TypeDef: TypeIndex: 0x%x, TypeName: %s.%s\n",
//               typeDef->_index,
//               typeDef->_typeNamespace,
//               typeDef->_typeName);
        break;
      }
      case TableIndexTypeRef: {
//        auto typeRef = _mainMetadata->GetTypeRefTable().GetRowAt(rowIndex);
//        printf("  -> TypeRef: TypeRefIndex: 0x%x, TypeName: %s.%s\n",
//               rowIndex,
//               _mainMetadata->GetString(typeRef.TypeNamespace),
//               _mainMetadata->GetString(typeRef.TypeName));
        break;
      }
      case TableIndexTypeSpec: {
        printf("  -> TypeSpec\n");
        const GenericInstanceType *genericInstType = dynamic_cast<const GenericInstanceType *>(GetTypeFromTypeSpec(rowIndex));
        if (genericInstType) {
          printf("%s\n", genericInstType->_genericClass->GetTypeName());
        }
        break;
      }
      default:
        break;
    }
  }
}

const TypeDefinition *ModelBuilder::GetTypeDefinitionFromTypeDef(TableRowIndex typeDefRowIndex, const GenericContext &context) {
  if (_typeDefinitions.at(NATURE_IDX(typeDefRowIndex)) != nullptr) {
//    const auto *a = _typeDefinitions.at(NATURE_IDX(typeDefRowIndex));
//    printf("Build typeDef: 0x%x, Namespace: %s, Name: %s cached\n", typeDefRowIndex, a->_typeNamespace, a->_typeName);
    return _typeDefinitions.at(NATURE_IDX(typeDefRowIndex));
  }

  auto &typeDefRow = _mainMetadata->GetTypeDefTable().GetRowAt(typeDefRowIndex);
  TypeDefinition *typeDef = new TypeDefinition();
  typeDef->_index = typeDefRowIndex;

  typeDef->_typeName = _mainMetadata->GetString(typeDefRow.TypeName);
  typeDef->_typeNamespace = _mainMetadata->GetString(typeDefRow.TypeNamespace);

//  printf("Build typeDef: 0x%x, Namespace: %s, Name: %s\n", typeDefRowIndex, typeDef->_typeNamespace, typeDef->_typeName);

  switch (BIT_MASK(typeDefRow.Flags, TypeAttributes::VisibilityMask)) {
    case TypeAttributes::NotPublic:
      typeDef->_visibility = TypeDefinitionVisibility::NotPublic;
      break;
    case TypeAttributes::Public:
      typeDef->_visibility = TypeDefinitionVisibility::Public;
      break;
    case TypeAttributes::NestedPublic:
      typeDef->_visibility = TypeDefinitionVisibility::NestedPublic;
      break;
    case TypeAttributes::NestedPrivate:
      typeDef->_visibility = TypeDefinitionVisibility::NestedPrivate;
      break;
    case TypeAttributes::NestedFamily:
      typeDef->_visibility = TypeDefinitionVisibility::NestedFamily;
      break;
    case TypeAttributes::NestedAssembly:
      typeDef->_visibility = TypeDefinitionVisibility::NestedAssembly;
      break;
    case TypeAttributes::NestedFamANDAssem:
      typeDef->_visibility = TypeDefinitionVisibility::NestedFamANDAssem;
      break;
    case TypeAttributes::NestedFamORAssem:
      typeDef->_visibility = TypeDefinitionVisibility::NestedFamORAssem;
      break;
    default:
      throw "Unsupported!";
  }

  switch (BIT_MASK(typeDefRow.Flags, TypeAttributes::ClassSemanticsMask)) {
    case TypeAttributes::Interface:
      typeDef->_isInterface = true;
      break;
    default:
      break;
  }
  switch (BIT_MASK(typeDefRow.Flags, TypeAttributes::LayoutMask)) {
    case TypeAttributes::AutoLayout:
      typeDef->_layout = ClassLayouts::AutoLayout;
      break;
    case TypeAttributes::SequentialLayout:
      typeDef->_layout = ClassLayouts::SequentialLayout;
      break;
    case TypeAttributes::ExplicitLayout:
      typeDef->_layout = ClassLayouts::ExplicitLayout;
      break;
    default:
      throw "Unexpected layout";
  }

  typeDef->_isAbstract = BIT_TEST(typeDefRow.Flags, TypeAttributes::Abstract);
  typeDef->_isSealed = BIT_TEST(typeDefRow.Flags, TypeAttributes::Sealed);

  // Parse generic.
  GetGenericParametersForType(typeDefRowIndex, typeDef->_genericContainer, _mainMetadata);
  GenericContext classGenericCtx = {.classContainer = &typeDef->_genericContainer};

  // Cache it, prevent recursion.
  _typeDefinitions[NATURE_IDX(typeDefRowIndex)] = typeDef;

  // Parse nested class.
  TableIndex declaringIndex = _mainMetadata->GetDeclaringType(typeDefRowIndex);
  if (declaringIndex != TableIndexInvalid) {
    typeDef->_declaringType = GetTypeDefinitionFromTypeDef(declaringIndex);
  }

  // Parse interfaces.
  const auto &interfaceIndexes = _mainMetadata->GetInterfaceImplsForType(typeDefRowIndex);
  if (!interfaceIndexes.empty()) {
//    printf("Type: 0x%x, %s.%s\n", typeDef->_index, typeDef->_typeNamespace, typeDef->_typeName);
    for (TypeDefOrRefCodedIndex itfIndex : interfaceIndexes) {
      const IType *itfType = GetTypeFromCodedIndex(itfIndex, classGenericCtx);
      typeDef->_interfaces.push_back(itfType);
//      if (auto clz = dynamic_cast<const GenericInstanceType *>(itfType)) {
//        printf("  itf GenericInstanceType: %s.%s\n", clz->_genericClass->GetTypeNamespace(), clz->_genericClass->GetTypeName());
//      } else if (auto clz = dynamic_cast<const UserType *>(itfType)) {
//        printf("  itf UserType %s.%s\n", clz->GetTypeDef()->GetTypeNamespace(), clz->GetTypeDef()->GetTypeName());
//      }
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
    const auto &fieldRow = _mainMetadata->GetFieldTable().GetRowAt(fieldRowIndex);
    Field *field = new Field();
    typeDef->_fields.push_back(field);
    switch (BIT_MASK(fieldRow.Flags, FieldAttributes::FieldAccessMask)) {
      case FieldAttributes::CompilerControlled:
        field->_access = FieldAccess::CompilerControlled;
        break;
      case FieldAttributes::Private:
        field->_access = FieldAccess::Private;
        break;
      case FieldAttributes::FamANDAssem:
        field->_access = FieldAccess::FamANDAssem;
        break;
      case FieldAttributes::Assembly:
        field->_access = FieldAccess::Assembly;
        break;
      case FieldAttributes::Family:
        field->_access = FieldAccess::Family;
        break;
      case FieldAttributes::FamORAssem:
        field->_access = FieldAccess::FamORAssem;
        break;
      case FieldAttributes::Public:
        field->_access = FieldAccess::Public;
        break;
      default:
        throw "Unexpected field access";
    }
    field->_index = fieldRowStartIndex;
    field->_isLiteral = BIT_TEST(fieldRow.Flags, FieldAttributes::Literal);
    field->_isInitOnly = BIT_TEST(fieldRow.Flags, FieldAttributes::InitOnly);
    field->_isStatic = field->_isLiteral || BIT_TEST(fieldRow.Flags, FieldAttributes::Static);
    field->_name = _mainMetadata->GetString(fieldRow.Name);
//    printf("Type: %s, Field: Index: 0x%x, Name: %s, Signature: %08x\n", typeDef->_typeName, fieldRowIndex, field->_name, fieldRow.Signature);
    field->_type = GetFieldType(fieldRow.Signature, classGenericCtx);
  }

  for (TableRowIndex methodRowIndex = methodRowStartIndex; methodRowIndex < methodRowStartIndex + methodsCount; ++methodRowIndex) {
    const auto &methodRow = _mainMetadata->GetMethodDefTable().GetRowAt(methodRowIndex);
    Method *method = new Method();
    typeDef->_methods.push_back(method);
    _methods[NATURE_IDX(methodRowIndex)] = method;

    method->_typeDefinition = typeDef;
    method->_name = _mainMetadata->GetString(methodRow.Name);
    switch (BIT_MASK(methodRow.Flags, MethodAttributes::MemberAccessMask)) {
      case MethodAttributes::CompilerControlled:
        method->_access = MemberAccess::CompilerControlled;
        break;
      case MethodAttributes::Private:
        method->_access = MemberAccess::Private;
        break;
      case MethodAttributes::FamANDAssem:
        method->_access = MemberAccess::FamANDAssem;
        break;
      case MethodAttributes::Assem:
        method->_access = MemberAccess::Assem;
        break;
      case MethodAttributes::Family:
        method->_access = MemberAccess::Family;
        break;
      case MethodAttributes::FamORAssem:
        method->_access = MemberAccess::FamORAssem;
        break;
      case MethodAttributes::Public:
        method->_access = MemberAccess::Public;
        break;
    }
    method->_isStatic = BIT_TEST(methodRow.Flags, MethodAttributes::Static);
    method->_isFinal = BIT_TEST(methodRow.Flags, MethodAttributes::Final);
    method->_isVirtual = BIT_TEST(methodRow.Flags, MethodAttributes::Virtual);
    method->_isAbstract = BIT_TEST(methodRow.Flags, MethodAttributes::Abstract);

    // Get generic parameters if needed.
    GetGenericParametersForMethod(methodRowIndex, method->_genericContainer, _mainMetadata);

    // II.23.2.1 MethodDefSig
    BlobReader methodSigReader = GetBlobReader(methodRow.Signature);
    SignatureAbbreviation abbr = methodSigReader.Read<SignatureAbbreviation>();
    bool hasThis = false;
    bool explicitThis = false;
    bool dft = false;
    bool varArg = false;
    bool generic = false;
    uint32_t genParamCount = 0;

    // The first byte of the Signature holds bits for HASTHIS, EXPLICITTHIS and calling convention (DEFAULT, VARARG, or GENERIC). These are ORed together.
    if (BIT_TEST(abbr, SignatureAbbreviation::HASTHIS)) {
      hasThis = true;
      if (BIT_TEST(abbr, SignatureAbbreviation::EXPLICITTHIS)) {
        explicitThis = true;
      }
    }

    if (BIT_TEST(abbr, SignatureAbbreviation::DEFAULT)) {
      dft = true;
    } else if (BIT_TEST(abbr, SignatureAbbreviation::VARARG)) {
      varArg = true;
    } else if (BIT_TEST(abbr, SignatureAbbreviation::GENERIC)) {
      generic = true;
      genParamCount = methodSigReader.ReadCompressedUInt32();
    }

    uint32_t sigParamCount = methodSigReader.ReadCompressedUInt32();

//    printf("Method: 0x%x, %s, hasThis: %d, explicitThis: %d, default: %d, varArg: %d, generic: %d, genParamCount: %d, paramCount: %d\n",
//           methodRowIndex, method->_name, hasThis, explicitThis, dft, varArg, generic, genParamCount, sigParamCount);
    // II.23.2.11 RetType
    GenericContext methodGenericCtx = {.classContainer = classGenericCtx.classContainer,
                                       .methodContainer = &method->_genericContainer};
    IType *retType = GetTypeFromBlob(methodSigReader, methodGenericCtx);
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
    TableRowIndex paramRowIndex = paramRowStartIndex;
    for (int i=0; i<sigParamCount; ++i) {
      Parameter param;
      param._type = GetTypeFromBlob(methodSigReader, methodGenericCtx);

      ParamTableRow paramRow = _mainMetadata->GetParamTable().GetRowAt(paramRowIndex);
      param._name = _mainMetadata->GetString(paramRow.Name);
      param._in = BIT_TEST(paramRow.Flags, ParamAttributes::In);
      param._out = BIT_TEST(paramRow.Flags, ParamAttributes::Out);
      param._optional = BIT_TEST(paramRow.Flags, ParamAttributes::Optional);

      // Prepare to fetch next.
      paramRowIndex ++;

      method->_parameters.push_back(std::move(param));
    }

    // Method specs.


//    if (strcmp(method->_name, "<Parabola>g__Func|18_0") == 0) {
////      exit(0);
//    }
//    exit(0);
  }

//  printf("%s, fields: %d, methods: %d\n", typeDef->_name, fieldsCount, methodsCount);
//  printf("TypeName: %s, namespace: %s, isGeneric: %d\n", typeDef->_name, typeDef->_namespace, typeDef->_isGeneric);

  return typeDef;
}

const ITypeDefinition *ModelBuilder::GetTypeDefinitionFromCodedIndex(const TypeDefOrRefCodedIndex &codedIndex, const GenericContext &context) {
  switch (codedIndex.GetTableIndex()) {
    case TableIndexTypeDef:
      return GetTypeDefinitionFromTypeDef(codedIndex.GetRowIndex(), context);
    case TableIndexTypeRef:
      return GetTypeDefinitionFromTypeRef(codedIndex.GetRowIndex(), context);
    case TableIndexTypeSpec:
      return GetTypeDefinitionFromTypeSpec(codedIndex.GetRowIndex(), context);
    default:
      throw "not supported!";
  }
}

const ITypeDefinition *ModelBuilder::GetTypeDefinitionFromTypeRef(TableRowIndex typeRefRowIndex, const GenericContext &context) {
  const TypeRefTableRow &row = _mainMetadata->GetTypeRefTable().GetRowAt(typeRefRowIndex);
  const char *typeName = _mainMetadata->GetString(row.TypeName);
  const char *typeNamespace = _mainMetadata->GetString(row.TypeNamespace);
//  printf("GetFromTypeRef, %d, %s, %s\n", row.ResolutionScope.GetTableIndex(), typeNamespace, typeName);

  switch (row.ResolutionScope.GetTableIndex()) {
    case TableIndexModule: {
//      printf("** Module\n");
      throw "Unsupported!";
      break;
    }
    case TableIndexModuleRef: {
//      printf("** ModuleRef\n");
      throw "Unsupported!";
      break;
    }
    case TableIndexAssemblyRef: {
//      printf("** AssemblyRef\n");
      const auto *assemblyRef = _assemblyRefs[NATURE_IDX(row.ResolutionScope.GetRowIndex())];
      const TypeDefinitionRef *typeDefRef = GetTypeDefinitionRef(assemblyRef, nullptr, typeNamespace, typeName);
      if (typeDefRef == nullptr)
        throw "Unknown type ref";
      return typeDefRef;
    }
    case TableIndexTypeRef: {
//      printf("** TypeRef\n");
      UserType *enclosingType = static_cast<UserType *>(GetTypeFromTypeRef(row.ResolutionScope.GetRowIndex()));
      const auto *assemblyRef = static_cast<const AssemblyRef *>(enclosingType->GetTypeDef()->GetAssembly());
      const TypeDefinitionRef *typeDefRef = GetTypeDefinitionRef(assemblyRef, enclosingType->GetTypeDef(), typeNamespace, typeName);
      if (typeDefRef == nullptr)
        throw "Unknown type ref";
      return typeDefRef;
    }
    default:
      throw "Unsupported!";
  }
}

const ITypeDefinition *ModelBuilder::GetTypeDefinitionFromTypeSpec(TableRowIndex typeSpecRowIndex, const GenericContext &context) {
  UserType *userType = dynamic_cast<UserType *>(GetTypeFromTypeSpec(typeSpecRowIndex, context));
  return userType->GetTypeDef();
}

const TypeDefinitionRef *
ModelBuilder::GetTypeDefinitionRef(const Models::AssemblyRef *assemblyRef,
                                   const Models::ITypeDefinition *__nullable declaringType,
                                   const char *typeNamespace, const char *typeName) {
  for (const auto *typeDefRef : assemblyRef->_typeDefinitions) {
    if (declaringType != nullptr && declaringType != typeDefRef->_declaringType)
      continue;

    if (strcmp(typeDefRef->_typeNamespace, typeNamespace) == 0 && strcmp(typeDefRef->_typeName, typeName) == 0) {
      return typeDefRef;
    }
  }
  printf("Warning no type matched in refs.\n");
  return nullptr;
}

void ModelBuilder::GetGenericParametersForType(TableRowIndex typeDefRowIndex, GenericContainer &container, const Metadata *metadata) {
  const vector<TableRowIndex> &genericParamRowIndexes = metadata->GetGenericParamsForType(typeDefRowIndex);
  if (!genericParamRowIndexes.empty()) {
    container.resize(genericParamRowIndexes.size());
    for (TableRowIndex genericParamRowIndex : genericParamRowIndexes) {
      const auto &genericParamRow = metadata->GetGenericParamTable().GetRowAt(genericParamRowIndex);
      container[genericParamRow.Number] = metadata->GetString(genericParamRow.Name);
    }
  }
}

void ModelBuilder::GetGenericParametersForMethod(TableRowIndex methodDefRowIndex, GenericContainer &container, const Metadata *metadata) {
  const vector<TableRowIndex> &genericParamRowIndexes = metadata->GetGenericParamsForMethod(methodDefRowIndex);
  if (!genericParamRowIndexes.empty()) {
    container.resize(genericParamRowIndexes.size());
    for (TableRowIndex genericParamRowIndex : genericParamRowIndexes) {
      const auto &genericParamRow = metadata->GetGenericParamTable().GetRowAt(genericParamRowIndex);
      container[genericParamRow.Number] = metadata->GetString(genericParamRow.Name);
    }
  }
}


IType *ModelBuilder::GetFieldType(BlobIndex signatureIndex, const GenericContext &context)  {
  BlobReader reader = GetBlobReader(signatureIndex);
  SignatureAbbreviation abbr = reader.Read<SignatureAbbreviation>();
  assert(BIT_TEST(abbr, SignatureAbbreviation::FIELD) && "Not field signature.");
  return GetTypeFromBlob(reader, context);
}

IType *ModelBuilder::GetTypeFromBlob(BlobReader &reader, const GenericContext &context) {
  // II.23.2.12 Type
  // Type is encoded in signatures as follows (I1 is an abbreviation for
  // ELEMENT_TYPE_I1, U1 is an abbreviation for ELEMENT_TYPE_U1, and so on;
  // see II.23.1.16):
  ELEMENT_TYPE eleType = reader.Read<ELEMENT_TYPE>();
//  printf("Type: 0x%02x\n", eleType);
  switch (eleType) {
    // BOOLEAN | CHAR | I1 | U1 | I2 | U2 | I4 | U4 | I8 | U8 | R4 | R8 | I | U
    case ELEMENT_TYPE::VOID:
      return new Types::Void;
    case ELEMENT_TYPE::BOOLEAN:
      return new Types::Boolean;
    case ELEMENT_TYPE::CHAR:
      return new Types::Char;
    case ELEMENT_TYPE::I1:
      return new Types::I1;
    case ELEMENT_TYPE::U1:
      return new Types::U1;
    case ELEMENT_TYPE::I2:
      return new Types::I2;
    case ELEMENT_TYPE::U2:
      return new Types::U2;
    case ELEMENT_TYPE::I4:
      return new Types::I4;
    case ELEMENT_TYPE::U4:
      return new Types::U4;
    case ELEMENT_TYPE::I8:
      return new Types::I8;
    case ELEMENT_TYPE::U8:
      return new Types::U8;
    case ELEMENT_TYPE::R4:
      return new Types::R4;
    case ELEMENT_TYPE::R8:
      return new Types::R8;
    case ELEMENT_TYPE::I:
      return new Types::I;
    case ELEMENT_TYPE::U:
      return new Types::U;
    case ELEMENT_TYPE::STRING:
      // | STRING
      return new StringType;
    case ELEMENT_TYPE::OBJECT:
      // | OBJECT
      return new ObjectType;
    case ELEMENT_TYPE::TYPEDBYREF:
      return new TypedByRefType;
    case ELEMENT_TYPE::VALUETYPE: {
      // | VALUETYPE TypeDefOrRefOrSpecEncoded
      TypeDefOrRefOrSpecEncoded encoded(reader.ReadCompressedUInt32());
      return GetTypeFromTypeDefOrRefOrSpecEncoded(encoded, context);
    }
    case ELEMENT_TYPE::CLASS: {
      // | CLASS TypeDefOrRefOrSpecEncoded
      TypeDefOrRefOrSpecEncoded encoded(reader.ReadCompressedUInt32());
      return GetTypeFromTypeDefOrRefOrSpecEncoded(encoded, context);
    }
    case ELEMENT_TYPE::GENERICINST: {
      // | GENERICINST (CLASS | VALUETYPE) TypeDefOrRefOrSpecEncoded GenArgCount Type *
      const UserType *genericClass = static_cast<UserType *>(GetTypeFromBlob(reader));
      const GenericContainer &selfGenericContainer = genericClass->GetTypeDef()->GetGenericContainer();
      uint32_t genArgCount = reader.ReadCompressedUInt32();

      assert(genArgCount == selfGenericContainer.size() && "Inflated argc does not matched.");

      GenericInstanceType *genericInstanceType = new GenericInstanceType(genericClass->GetTypeDef());
      for (int i = 0; i < genArgCount; ++i) {
        IType *inflatedType = GetTypeFromBlob(reader, context);
        InflatedVariable inflatedVariable;
        inflatedVariable._isMethod = false;
        inflatedVariable._name = selfGenericContainer.at(i);
        inflatedVariable._type = inflatedType;
        genericInstanceType->_inflatedVariables.push_back(std::move(inflatedVariable));
      }

      return genericInstanceType;
    }
//    case ELEMENT_TYPE::FNPTR: {
//      // | FNPTR MethodDefSig
//      // | FNPTR MethodRefSig
//    }
    case ELEMENT_TYPE::VAR: {
      // | MVAR number
      uint32_t number = reader.ReadCompressedUInt32();
//      printf("VAR Number: %d\n", number);
      if (context.classContainer == nullptr)
        throw "Unsupported!";
      return new GenericParameterType(context.classContainer->at(number));
    }
    case ELEMENT_TYPE::MVAR: {
      // | MVAR number
      uint32_t number = reader.ReadCompressedUInt32();
      if (context.methodContainer == nullptr)
        throw "Unsupported!";
      return new GenericParameterType(context.methodContainer->at(number));
    }
    case ELEMENT_TYPE::SZARRAY: {
      // | SZARRAY CustomMod* Type (single dimensional, zero-based array i.e., vector)
      IType *eleType = GetTypeFromBlob(reader, context);
      return new ArrayType(eleType);
    }
    case ELEMENT_TYPE::ARRAY: {
      // ARRAY Type ArrayShape (general array, see §II.23.2.13)
      IType *eleType = GetTypeFromBlob(reader, context);
      uint32_t rank = reader.ReadCompressedUInt32();
      uint32_t numSizes = reader.ReadCompressedUInt32();
      for (int i = 0; i < numSizes; ++i) {
        uint32_t size = reader.ReadCompressedUInt32();
      }
      uint32_t numLowBounds = reader.ReadCompressedUInt32();
      for (int i = 0; i < numLowBounds; ++i) {
        uint32_t lowBound = reader.ReadCompressedUInt32();
      }
      return new ArrayType(eleType);
    }
//    case ELEMENT_TYPE::PTR: {
//      // | PTR CustomMod* Type
//      // | PTR CustomMod* VOID
//    }
    case ELEMENT_TYPE::BYREF: {
      IType *pointeeType = GetTypeFromBlob(reader, context);
      return new PointerType(pointeeType);
    }
    case ELEMENT_TYPE::CMOD_OPT: {
      // TODO CMOD_OPT
      printf("CMOD_OPT\n");
      uint32_t encodedToken = reader.ReadCompressedUInt32();
      return GetTypeFromBlob(reader, context);
    }
    default:
      throw "Unsupported";
  }
}

IType *ModelBuilder::GetTypeFromCodedIndex(const TypeDefOrRefCodedIndex &codedIndex,
                                           const GenericContext &context) {
  switch (codedIndex.GetTableIndex()) {
    case TableIndexTypeDef:
      return GetTypeFromTypeDef(codedIndex.GetRowIndex(), context);
    case TableIndexTypeRef:
      return GetTypeFromTypeRef(codedIndex.GetRowIndex(), context);
    case TableIndexTypeSpec:
      return GetTypeFromTypeSpec(codedIndex.GetRowIndex(), context);
    default:
      throw "not supported!";
  }
}

IType *ModelBuilder::GetTypeFromTypeRef(TableRowIndex typeRefRowIndex, const GenericContext &context) {
  const ITypeDefinition *ref = GetTypeDefinitionFromTypeRef(typeRefRowIndex, context);
  return new UserType(ref);
}

IType *ModelBuilder::GetTypeFromTypeDef(TableRowIndex typeDefRowIndex, const GenericContext &context) {
  const TypeDefinition *def = GetTypeDefinitionFromTypeDef(typeDefRowIndex, context);
  return new UserType(def);
}

IType *ModelBuilder::GetTypeFromTypeSpec(TableRowIndex typeSpecRowIndex, const GenericContext &context) {
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
  const auto &row = _mainMetadata->GetTypeSpecTable().GetRowAt(typeSpecRowIndex);
  BlobReader reader = GetBlobReader(row.Signature);
  return GetTypeFromBlob(reader, context);
}

IType *ModelBuilder::GetTypeFromTypeDefOrRefOrSpecEncoded(const TypeDefOrRefOrSpecEncoded &encoded, const GenericContext &context) {
  TableRowIndex rowIndex = encoded.GetRowIndex();
  switch (encoded.GetTableIndex()) {
    case TableIndexTypeDef: {
      return GetTypeFromTypeDef(rowIndex, context);
    }
    case TableIndexTypeRef: {
      return GetTypeFromTypeRef(rowIndex, context);
    }
    case TableIndexTypeSpec: {
      return GetTypeFromTypeSpec(rowIndex, context);
    }
    default:
      throw "Unsupported";
  }
}

BlobReader ModelBuilder::GetBlobReader(BlobIndex index) {
//  // II.24.2.4 #US and #Blob heaps
  BlobReader reader(_mainMetadata->GetBlobPointer<const uint8_t *>(index));
  uint32_t seqLength = reader.ReadCompressedUInt32();
  return reader;
}

// II.23.2 Blobs and signatures
// MethodRefSig (differs from a MethodDefSig only for VARARG calls)
// MethodDefSig
// FieldSig
// PropertySig
// LocalVarSig
// TypeSpec
// MethodSpec