//
// Created by admin on 2024/3/8.
//

#ifndef DOTNETFILEPARSER_MODELS_H
#define DOTNETFILEPARSER_MODELS_H

#include <vector>
#include <string>
#include <map>
#include <type_traits>

#include "metadata.h"

namespace Models {
  struct TypeDefinition;
  struct ModelBuilder;

  struct IType {
    virtual ~IType() {}
  };

  template<class T>
  struct PrimitiveType: IType {
    using RawType = T;
    constexpr static uint8_t TypeSize = sizeof(RawType);
  };

  namespace PrimitiveTypes {
    typedef PrimitiveType<int8_t>     S1;
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
    uint16_t GetRank() { return _rank; }
    const IType &GetElementType() const { return _eleType; }

  private:
    uint16_t _rank;
    IType _eleType;

    friend class ModelBuilder;
  };

  struct ClassType: IType {
  public:
    const TypeDefinition &GetClass() const { return _class; }

  private:
    const TypeDefinition &_class;
    ClassType(const TypeDefinition &klass): _class(klass) {}

    friend class ModelBuilder;
  };

  struct GenericParameterType: IType {
  public:
    const char *GetName() const { return _name; }
  private:
    const char *_name;

    friend class ModelBuilder;
  };

  struct InflatedVariable {
  public:
    const char *GetName() const { return _name; }
    const IType &GetType() const { return _type; }
    bool GetIsMethod() const { return _isMethod; }

  private:
    const char *_name;
    const IType _type;
    bool _isMethod;

    friend class ModelBuilder;
  };

  struct GenericInstanceType: IType {

  public:
    const std::vector<InflatedVariable> &GetInflatedVariables() const { return _inflatedVariables; }
    const TypeDefinition &GetGenericClass() const { return _genericClass; }

  private:
    GenericInstanceType(const TypeDefinition &genericClass): _genericClass(genericClass) {}
    const std::vector<InflatedVariable> _inflatedVariables;
    const TypeDefinition &_genericClass;

    friend class ModelBuilder;
  };

  struct PointerType: IType {
  public:
    const IType &GetPointeeType() const { return _pointeeType; }
  private:
    PointerType(const IType &pointeeType): _pointeeType(pointeeType) {}

    const IType& _pointeeType;

    friend class ModelBuilder;
  };

  struct Field {
  public:
    const char *GetName() const { return _name; }
    const IType &GetType() const { return _type; }
    bool GetIsStatic() const { return _isStatic; }

  private:
    const char *_name;
    const IType &_type;
    bool _isStatic;
    Field(const char *name, const IType &type): _name(name), _type(type) {}

    friend class ModelBuilder;
  };

  struct Parameter {
  public:
    const char *GetName() const { return _name; }
    const IType &GetType() const { return _type; }
    bool GetIn() const { return _in; }
    bool GetOut() const { return _out; }
    bool GetOptional() const { return _optional; }

  private:
    bool _in;
    bool _out;
    bool _optional;
    const char *_name;
    const IType _type;

    Parameter(const char *name, const IType &type): _name(name), _type(type) {}

    friend class ModelBuilder;
  };

  struct Method {

  public:
    const char *GetName() const { return _name; }
    const IType &GetReturnType() const { return _returnType; }
    bool GetIsStatic() const { return _isStatic; }
    bool GetIsVirtual() const { return _isVirtual; }
    bool GetIsGeneric() const { return _isGeneric; }
    const std::vector<const char *> &GetGenericParameterNames() const { return _genericParameterNames; }
    const std::vector<Parameter> &GetParameters() const { return _parameters; }

  private:
    const char *_name;

    bool _isStatic;
    bool _isVirtual;
    bool _isGeneric;

    IType _returnType;
    std::vector<const char *> _genericParameterNames;
    std::vector<Parameter> _parameters;

    friend class ModelBuilder;
  };

  enum ClassLayoutAttributes {
    AutoLayout = 0x00000000,
    SequentialLayout = 0x00000008,
    ExplicitLayout = 0x00000010
  };

  struct TypeDefinition {
    const char *GetName() const { return _name; }
    const char *GetNamespace() const { return _namespace; }
    const std::vector<const char *> &GetGenericParameterNames() const { return _genericParameterNames; }
    const std::vector<Field *> &GetFields() const { return _fields; }
    const std::vector<Method *> &GetMethods() const { return _methods; }


    TypeDefinition() {}
    ~TypeDefinition();
  private:
    const char *_name;
    const char *_namespace;

    bool _isPublic;
    bool _isPrivate;
    bool _isFamily;
    bool _isAssembly;
    bool _isNestedFamANDAssem;
    bool _isFamORAssem;

    bool _isInterface;
    bool _isAbstract;
    bool _isSealed;

    std::vector<const char *> _genericParameterNames;
    std::vector<Field *> _fields;
    std::vector<Method *> _methods;

    friend class ModelBuilder;
  };

  struct Assembly {
  public:
    const char *GetName() const { return _name; }
    const char *GetCulture() const { return _culture; }
    const std::vector<TypeDefinition *> &TypeDefinitions() const { return _typeDefinitions; }

    ~Assembly();
  private:
    const char *_name;
    const char *_culture;

    std::vector<TypeDefinition *> _typeDefinitions;

    friend class ModelBuilder;

  };

  struct ModelBuilder {
    const Metadata &_metadata;
    ModelBuilder(const Metadata &metadata): _metadata(metadata) {}

    void Build();

  private:
    TypeDefinition *BuildTypeDefinition(TableRowIndex i);
  };
}

#endif //DOTNETFILEPARSER_MODELS_H
