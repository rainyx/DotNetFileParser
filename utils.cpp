//
// Created by admin on 2024/4/22.
//

#include "utils.h"

namespace Utils {
  bool StringReplace(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
      return false;
    str.replace(start_pos, from.length(), to);
    return true;
  }
}

