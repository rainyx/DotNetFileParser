//
// Created by admin on 2024/4/22.
//

#ifndef DOTNETFILEPARSER_UTILS_H
#define DOTNETFILEPARSER_UTILS_H
#include <string>

namespace Utils {
  bool StringReplace(std::string& str, const std::string& from, const std::string& to);
  template<class T>
  static bool BitTest(T a, T b) {
    return (static_cast<std::underlying_type_t<T>>(a) & static_cast<std::underlying_type_t<T>>(b)) != 0;
  }

  template<class T>
  static T BitMask(T a, T b) {
    auto v = static_cast<std::underlying_type_t<T>>(a) & static_cast<std::underlying_type_t<T>>(b);
    return static_cast<T>(v);
  }
}


#endif //DOTNETFILEPARSER_UTILS_H
