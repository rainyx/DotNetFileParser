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

  const char *ReadNullTermString(uint32_t *readLen = nullptr) {
    uint32_t oldBytesRead = _bytesRead;
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
  uint32_t GetBytesRead() const { return _bytesRead; }

private:
  const uint8_t *_data;
  uint32_t _bytesRead = 0;
};



#endif //DOTNETFILEPARSER_STREAM_H
