//
// Created by admin on 2024/3/7.
//

#ifndef DOTNETFILEPARSER_STREAM_H
#define DOTNETFILEPARSER_STREAM_H

#include <cstdint>

struct BinaryReader {

public:
  explicit BinaryReader(const uint8_t *data): _data(data) {}

  template<class T>
  const T Read() {
    const T *ptr = reinterpret_cast<const T *>(_data);
    _data += sizeof(T);
    _bytesRead += sizeof(T);
    return *ptr;
  }

  const char *ReadNullTermString(int *readLen = nullptr) {
    int oldBytesRead = _bytesRead;
    const char *origPtr = reinterpret_cast<const char *>(_data);
    do {
      char c = Read<char>();
      if (!c)
        break;
    } while (true);

    if (readLen != nullptr)
      *readLen = _bytesRead - oldBytesRead;

    return origPtr;
  }

  const uint8_t *GetData() const { return _data; }
  int GetBytesRead() const { return _bytesRead; }

private:
  const uint8_t *_data;
  int _bytesRead = 0;
};



#endif //DOTNETFILEPARSER_STREAM_H
