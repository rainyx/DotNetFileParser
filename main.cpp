#include <iostream>
#include <string>
#include <vector>
#include <map>



#include "common.h"
#include "metadata.h"
#include "models.h"

using namespace std;

int main() {
  const char *filePath = "/Users/admin/Desktop/TextAsset/MainScriptsHotUpdate.dll";

  auto builder = new Models::ModelBuilder(filePath);
  builder->Build();

  return 0;
}
