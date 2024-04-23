//
// Created by admin on 2024/3/10.
//

#ifndef DOTNETFILEPARSER_DOTNET_H
#define DOTNETFILEPARSER_DOTNET_H

#include <string>
#include <vector>

#include "common.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

struct PE {
private:
  const MZHeader *_mzHeader { nullptr };
  const PEHeader *_peHeader { nullptr };

  std::vector<Section> _sections;
  std::vector<DataDirectory> _directories;
public:
  explicit PE(const MZHeader *mzHeader): _mzHeader(mzHeader) {
    ParseHeader();
  }

  template<class T>
  T MapRVA(uint32_t rva) const {
    const Section &sec = GetSectionByRVA(rva);
    return reinterpret_cast<T>(GetRawPointer() + sec.pointerToRawData + (rva - sec.virtualAddress));
  }

  template<class T>
  T MapRVA(const DataDirectory &dir) const {
    return MapRVA<T>(dir.address);
  }

  const uint8_t *GetRawPointer() const {
    return reinterpret_cast<const uint8_t *>(_mzHeader);
  }

  const Section &GetSectionByRVA(uint32_t rva) const {
    for (const Section &sec : _sections) {
      if (rva >= sec.virtualAddress && rva <= sec.virtualAddress + sec.sizeOfRawData)
        return sec;
    }
    throw "No section with rva";
  }

  const Section &GetSectionByDirectory(const DataDirectory &dir) const {
    return GetSectionByRVA(dir.address);
  }

  const Section &GetSectionByName(const std::string &name) const {
    for (const Section &sec : _sections) {
      if (sec.name == name) {
        return sec;
      }
    }

    throw "No section with name";
  }

  const DataDirectory GetDataDirectory(uint32_t i) const {
    return _directories[i];
  }

private:
  void ParseHeader() {
    _peHeader = reinterpret_cast<const PEHeader *>(GetRawPointer() + _mzHeader->peHeaderPointer);

    uint32_t numOfDirs = _peHeader->windowsSpecificHeader.numberOfRvaAndSizes;
    // read directory
    auto dirs = reinterpret_cast<const DataDirectory *>(_peHeader + 1);

    for (int i=0; i<numOfDirs; ++i) {
      const DataDirectory &dir = dirs[i];
      _directories.push_back(dir);
    }

    // read sections
    uint32_t numOfSections = _peHeader->coffHeader.numberOfSections;
    auto secs = reinterpret_cast<const Section *>(dirs + numOfDirs);
    for (int i=0; i<numOfSections; ++i) {
      const Section &sec = secs[i];
      _sections.push_back(sec);
    }
  }

};

struct DotNetFile {
private:
  const std::string _filePath;
  struct stat _fileInfo { 0 };
  void *_fileData { nullptr };
  const MetadataHeader *_metadataHeader { nullptr };
  const CLRHeader *_clrHeader { nullptr };

public:
  explicit DotNetFile(const std::string &filePath): _filePath(filePath) {
    Parse();
  }

  const MetadataHeader *GetMetadataHeader() const { return _metadataHeader; }
  const CLRHeader *GetCLRHeader() const { return _clrHeader; }

private:
  void Parse() {
    int fd = open(_filePath.c_str(), O_RDONLY);
    fstat(fd, &_fileInfo);
    _fileData = mmap(nullptr, _fileInfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    PE pe(reinterpret_cast<const MZHeader *>(_fileData));

    const DataDirectory &clrDir = pe.GetDataDirectory(DataCLRHeader);
    const Section &clrSec = pe.GetSectionByDirectory(clrDir);

    _clrHeader = pe.MapRVA<const CLRHeader *>(clrDir);
    _metadataHeader = pe.MapRVA<const MetadataHeader *>(_clrHeader->MetaDataDirectoryAddress);
  }

  ~DotNetFile() {
    if (_fileData != nullptr) {
      munmap(_fileData, _fileInfo.st_size);
      _fileData = nullptr;
    }
  }
};

#endif //DOTNETFILEPARSER_DOTNET_H
