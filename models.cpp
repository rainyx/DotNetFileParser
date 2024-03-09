//
// Created by admin on 2024/3/8.
//

#include "models.h"

using namespace std;

using namespace Models;

TypeDefinition::~TypeDefinition() {
  for_each(_methods.begin(), _methods.end(), [](Method *method) {
    delete method;
  });
  _methods.clear();
  for_each(_fields.begin(), _fields.end(), [](Field *field) {
    delete field;
  });
  _fields.clear();
}

Assembly::~Assembly() {
  for_each(_typeDefinitions.begin(), _typeDefinitions.end(), [](TypeDefinition *typeDef) {
    delete typeDef;
  });
  _typeDefinitions.clear();
}

void ModelBuilder::Build() {
  // Build assembly
  Assembly assembly;
  const AssemblyTableRow &assemblyRow = _metadata.GetAssemblyTable().GetRowAt(0);
  assembly._name = _metadata.GetString(assemblyRow.Name);
  assembly._culture = _metadata.GetString(assemblyRow.Culture);

  // Build types def.
  for (TableRowIndex i=0; i<_metadata.GetTableRowsCount(TableIndexTypeDef); ++i) {
    TypeDefinition *typeDef = BuildTypeDefinition(i);
    assembly._typeDefinitions.push_back(std::move(typeDef));
  }
}

inline
template<class T>
static bool BIT_TEST(const T &a, const T &b) {
  return (static_cast<std::underlying_type<T>>(a) & static_cast<std::underlying_type<T>>(b)) != 0;
}

TypeDefinition *ModelBuilder::BuildTypeDefinition(TableRowIndex i) {
  auto &typeDefRow = _metadata.GetTypeDefTable().GetRowAt(i);
  TypeDefinition *typeDef = new TypeDefinition();
  typeDef->_name = _metadata.GetString(typeDefRow.TypeName);
  typeDef->_namespace = _metadata.GetString(typeDefRow.TypeNamespace);

  typeDef->_isPublic = BIT_TEST(typeDefRow.Flags, TypeAttributes::Public);

  printf("TypeName: %s, namespace: %s\n", typeDef->_name, typeDef->_namespace);
  return typeDef;
}