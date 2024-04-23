//
// Created by admin on 2024/3/8.
//

#ifndef DOTNETFILEPARSER_MODELS_H
#define DOTNETFILEPARSER_MODELS_H

#include <vector>
#include <string>
#include <map>
#include <type_traits>
#include <filesystem>
#include <memory>

#include "metadata.h"
#include "dotnet.h"

namespace Models {
  struct ITypeDefinition;
  struct TypeDefinition;
  struct ModelBuilder;

  enum class TypeType {
    Primitive,
    Class,
    Array,
    GenericInstance,
    GenericParameter,
    Pointer
  };

  struct IType {
    virtual ~IType() = default;
    virtual TypeType GetType() const = 0;
  };

  template<class T>
  struct PrimitiveType: IType {
    using RawType = T;
    TypeType GetType() const override { return TypeType::Primitive; };
    constexpr static uint8_t TypeSize = sizeof(RawType);
  };

  namespace Types {
    typedef PrimitiveType<void>       Void;
    typedef PrimitiveType<bool>       Boolean;
    typedef PrimitiveType<uint16_t>   Char;
    typedef PrimitiveType<int8_t>     I1;
    typedef PrimitiveType<uint8_t>    U1;
    typedef PrimitiveType<int16_t>    I2;
    typedef PrimitiveType<uint16_t>   U2;
    typedef PrimitiveType<int32_t>    I4;
    typedef PrimitiveType<uint32_t>   U4;
    typedef PrimitiveType<int64_t>    I8;
    typedef PrimitiveType<uint64_t>   U8;
    typedef PrimitiveType<float>      R4;
    typedef PrimitiveType<double>     R8;
    typedef PrimitiveType<intptr_t>   I;
    typedef PrimitiveType<uintptr_t>  U;
  }

  struct ArrayType: IType {
  public:
    TypeType GetType() const override { return TypeType::Array; };
    uint16_t GetRank() const { return _rank; }
    const IType *GetElementType() const { return _eleType; }

  private:
    explicit ArrayType(const IType *eleType): _rank(1), _eleType(eleType) {}

    uint16_t _rank;
    const IType *_eleType;

    friend class ModelBuilder;
  };

  // Class / ValueType / Enum
  struct ClassType: IType {
  public:
    TypeType GetType() const override { return TypeType::Class; };
    const ITypeDefinition *GetTypeDef() const { return _typeDef; }

  private:
    const ITypeDefinition *_typeDef;
    explicit ClassType(const ITypeDefinition *typeDef): _typeDef(typeDef) {}

    friend class ModelBuilder;
  };

  struct GenericParameterType: IType {
  public:
    const char *GetName() const { return _name; }
    TypeType GetType() const override { return TypeType::GenericParameter; };
  private:
    explicit GenericParameterType(const char *name): _name(name) {}
    const char *_name;

    friend class ModelBuilder;
  };

  struct InflatedVariable {
  public:
    const char *GetName() const { return _name; }
    const IType *GetType() const { return _type; }
    bool GetIsMethod() const { return _isMethod; }

  private:
    const char *_name;
    const IType *_type;
    bool _isMethod;

    friend class ModelBuilder;
  };

  struct GenericInstanceType: IType {

  public:
    const std::vector<InflatedVariable> &GetInflatedVariables() const { return _inflatedVariables; }
    const ITypeDefinition *GetGenericClass() const { return _genericClass; }
    TypeType GetType() const override { return TypeType::GenericInstance; };

  private:
    explicit GenericInstanceType(const ITypeDefinition *genericClass): _genericClass(genericClass) {}
    std::vector<InflatedVariable> _inflatedVariables;
    const ITypeDefinition *_genericClass;

    friend class ModelBuilder;
  };

  struct PointerType: IType {
  public:
    const IType *GetPointeeType() const { return _pointeeType; }
    TypeType GetType() const override { return TypeType::Pointer; };

  private:
    explicit PointerType(const IType *pointeeType): _pointeeType(pointeeType) {}

    const IType *_pointeeType;

    friend class ModelBuilder;
  };

  enum class FieldAccess {
    Public,
    Private,
    Family,
    CompilerControlled,
    FamANDAssem,
    FamORAssem,
    Assembly,
  };

  struct Field {
  public:
    const char *GetName() const { return _name; }
    const IType *GetType() const { return _type; }
    bool GetIsStatic() const { return _isStatic; }

  private:
    TableRowIndex _rowIndex;
    const char *_name;
    const IType *_type;
    bool _isStatic;
    bool _isLiteral;
    bool _isInitOnly;

    FieldAccess _access;

    friend class ModelBuilder;
  };

  struct Parameter {
  public:
    const char *GetName() const { return _name; }
    const IType *GetType() const { return _type; }
    bool GetIn() const { return _in; }
    bool GetOut() const { return _out; }
    bool GetOptional() const { return _optional; }

  private:
    bool _in;
    bool _out;
    bool _optional;

    const char *_name;
    const IType *_type;

    friend class ModelBuilder;
  };

  enum class MemberAccess {
    CompilerControlled,
    Private,
    FamANDAssem,
    Assem,
    Family,
    FamORAssem,
    Public
  };

  typedef std::vector<const char *> GenericContainer;

  struct Method {

  public:
    const char *GetName() const { return _name; }
    const IType *GetReturnType() const { return _returnType; }
    bool GetIsStatic() const { return _isStatic; }
    bool GetIsVirtual() const { return _isVirtual; }
    bool GetIsGeneric() const { return !_genericContainer.empty(); }
    const std::vector<const char *> &GetGenericContainer() const { return _genericContainer; }
    const std::vector<Parameter> &GetParameters() const { return _parameters; }

  private:
    TableRowIndex _index;
    const char *_name;
    uint _encodedMethodIndex;
    bool _isStatic;
    bool _isVirtual;
    bool _isFinal;
    bool _isAbstract;
    bool _isGeneric;

    MemberAccess _access;

    const TypeDefinition *_typeDefinition;
    const IType *_returnType;
    GenericContainer _genericContainer;
    std::vector<Parameter> _parameters;

    friend class ModelBuilder;
  };

  enum class ClassLayouts {
    AutoLayout,
    SequentialLayout,
    ExplicitLayout
  };

  enum class TypeDefinitionVisibility {
    NotPublic,
    Public,
    NestedPublic,
    NestedPrivate,
    NestedFamily,
    NestedAssembly,
    NestedFamANDAssem,
    NestedFamORAssem
  };


  struct Assembly;
  struct AssemblyRef;

  struct IAssembly {
    virtual const char *GetName() const = 0;
    virtual TableRowIndex GetIndex() const = 0;
  };

  struct ITypeDefinition {
    virtual ~ITypeDefinition() = default;
    virtual const std::string &GetFullName() const = 0;
    virtual const std::string &GetOrigFullName() const = 0;
    virtual const IAssembly *GetAssembly() const = 0;
    virtual bool GetIsValueType() const = 0;
    virtual const GenericContainer &GetGenericContainer() const = 0;
    virtual const ITypeDefinition *GetDeclaringType() const = 0;
    virtual bool GetIsGeneric() const = 0;
  };

  struct TypeDefinitionBase: ITypeDefinition {
    virtual ~TypeDefinitionBase() {}
    const std::string &GetFullName() const override { return _fullName; }
    const std::string &GetOrigFullName() const override { return _origFullName; };
    const IAssembly *GetAssembly() const override { return _assembly; }
    bool GetIsValueType() const override { return _isValueType; }
    const GenericContainer &GetGenericContainer() const override { return _genericContainer; }
    const ITypeDefinition *GetDeclaringType() const override { return _declaringType; }
    bool GetIsGeneric() const override { return !GetGenericContainer().empty(); }

  private:
    TableRowIndex _rowIndex { TableIndexInvalid };
    uint32_t _typeIndex { 0 };
    IAssembly *_assembly {nullptr};
    std::string _fullName;
    std::string _origFullName;
    // Inherits from System.ValueType
    bool _isValueType {false};
    bool _isEnum { false };
    const ITypeDefinition *_declaringType {nullptr};
    const IType *_parent { nullptr };

    GenericContainer _genericContainer;

    friend class ModelBuilder;
  };

  struct TypeDefinitionRef: TypeDefinitionBase {
    TypeDefinitionRef() = default;
    ~TypeDefinitionRef() override = default;
  private:
    friend class ModelBuilder;
  };

  struct TypeDefinition: TypeDefinitionBase {
    const std::vector<Field *> &GetFields() const { return _fields; }
    const std::vector<Method *> &GetMethods() const { return _methods; }

    TypeDefinition() {}
    ~TypeDefinition();
  private:
    TypeDefinitionVisibility _visibility;
    bool _isInterface;
    bool _isAbstract;
    bool _isSealed;

    std::vector<Field *> _fields;
    std::vector<Method *> _methods;
    std::vector<const IType *> _interfaces;

    ClassLayouts _layout;

    friend class ModelBuilder;
  };

  struct Assembly: IAssembly {
  public:
    TableRowIndex GetIndex() const override { return _index; }
    const char *GetName() const override { return _name; }
    const char *GetCulture() const { return _culture; }
    const std::vector<const TypeDefinition *> &TypeDefinitions() const { return _typeDefinitions; }

    ~Assembly();
  private:
    TableRowIndex _index;
    const char *_name;
    const char *_culture;
    uint16_t _majorVersion;
    uint16_t _minorVersion;
    uint16_t _buildNumber;
    uint16_t _reversionNumber;

    std::vector<const TypeDefinition *> _typeDefinitions;

    friend class ModelBuilder;
  };

  struct AssemblyRef: IAssembly {
  public:
    TableRowIndex GetIndex() const override { return _index; }
    const char *GetName() const override { return _name; }
    const std::vector<TypeDefinitionRef *> &TypeDefinitions() const { return _typeDefinitions; }
  private:
    TableRowIndex _index;
    const char *_name;
    std::vector<TypeDefinitionRef *> _typeDefinitions;

    friend class ModelBuilder;
  };


  struct ModelBuilder {
    struct GenericContext {
      const GenericContainer *classContainer;
      const GenericContainer *methodContainer;
    };

    explicit ModelBuilder(const std::string &filePath);

    void Build();

    std::string GetTypeDefinitionFullName(TableRowIndex typeDefRowIndex, bool orig = false);
    static std::string GetTypeRefFullName(const std::string &assemblyName, const std::string &typeNamespace, const std::string &typeName);

    // For main
    const TypeDefinition *GetTypeDefinition(TableRowIndex typeDefRowIndex);
    const ITypeDefinition *GetTypeDefinitionFromTypeRef(TableRowIndex typeRefRowIndex, bool decay);
    const ITypeDefinition *GetTypeDefinitionFromTypeSpec(TableRowIndex typeSpecRowIndex, const GenericContext *context, bool decay);

    static const TypeDefinitionRef *GetTypeDefinitionRef(const AssemblyRef *assemblyRef, const ITypeDefinition *__nullable declaringType, const char *typeNamespace, const char *typeName);

    IType *GetType(BlobReader &reader, const GenericContext *context, bool decay);
    IType *GetType(const TypeDefOrRefCodedIndex &codedIndex, const GenericContext *__nullable context, bool decay);
    IType *GetType(const TypeDefOrRefOrSpecEncoded &encoded, const GenericContext *__nullable context, bool decay);
    IType *GetTypeFromTypeDef(TableRowIndex typeDefRowIndex, bool decay);
    IType *GetTypeFromTypeRef(TableRowIndex typeRefRowIndex, bool decay);
    IType *GetTypeFromTypeSpec(TableRowIndex typeSpecRowIndex, const GenericContext *__nullable context, bool decay);

    BlobReader GetBlobReader(BlobIndex index);

    static void GetGenericParametersForType(TableRowIndex typeDefRowIndex, GenericContainer &container, const Metadata *metadata);
    static void GetGenericParametersForMethod(TableRowIndex methodDefRowIndex, GenericContainer &container, const Metadata *metadata);

    Field *GetField(TypeDefinition *typeDef, TableRowIndex fieldRowIndex);
    Method *GetMethod(TypeDefinition *typeDef, TableRowIndex methodRowIndex);

  private:
    void PrepareAssemblyRef(TableRowIndex assemblyRowIndex, const Metadata *metadata);
    void PrepareTypeDefinitionRef(AssemblyRef *assemblyRef, TableRowIndex i, const Metadata *metadata);
    static bool IsValueType(TypeDefOrRefCodedIndex codedIndex, const Metadata *metadata);

    static int GetTypeDefTypeIndex(int typeDefIndex);
    static uint GetMethodDefEncodedIndex(int methodDefIndex);
    static uint EncodeHybridClrIndex(uint index);
    static uint EncodeIndex(uint usage, uint index);
  private:
    const std::filesystem::path _filePath;
    std::filesystem::path _workingDirectory;

    std::vector<const TypeDefinition *> _typeDefinitions;
    std::vector<const Method *> _methods;

    Metadata *_mainMetadata { nullptr };
    DotNetFile *_mainDotNetFile { nullptr };
    // for assembly refs.
    std::vector<AssemblyRef *> _assemblyRefs;
    std::vector<Metadata *> _refsMetadata;
    std::vector<DotNetFile *> _refsDotNetFile;

    Assembly *_mainAssembly = { nullptr };

    ClassType *_objectType { nullptr };
    ClassType *_stringType { nullptr };
    ClassType *_typedByRefType { nullptr };

    Types::Void *_voidType { nullptr };
    Types::Boolean *_booleanType { nullptr };
    Types::Char *_charType { nullptr };
    Types::U1 *_u1Type { nullptr };
    Types::I1 *_i1Type { nullptr };
    Types::U2 *_u2Type { nullptr };
    Types::I2 *_i2Type { nullptr };
    Types::U4 *_u4Type { nullptr };
    Types::I4 *_i4Type { nullptr };
    Types::U8 *_u8Type { nullptr };
    Types::I8 *_i8Type { nullptr };
    Types::R4 *_r4Type { nullptr };
    Types::R8 *_r8Type { nullptr };
    Types::I *_iType { nullptr };
    Types::U *_uType { nullptr };
  };
}

#endif //DOTNETFILEPARSER_MODELS_H
