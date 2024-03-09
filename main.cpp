#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "metadata.h"
#include "models.h"

using namespace std;
struct PE {
private:
  const MZHeader *_mzHeader;
  const PEHeader *_peHeader;

  std::vector<Section> _sections;
  std::vector<DataDirectory> _directories;
public:
  PE(const MZHeader *mzHeader): _mzHeader(mzHeader) {
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
    const DataDirectory *dirs = reinterpret_cast<const DataDirectory *>(_peHeader + 1);

    for (int i=0; i<numOfDirs; ++i) {
      const DataDirectory &dir = dirs[i];
      _directories.push_back(dir);
    }

    // read sections
    uint32_t numOfSections = _peHeader->coffHeader.numberOfSections;
    const Section *secs = reinterpret_cast<const Section *>(dirs + numOfDirs);

    for (int i=0; i<numOfSections; ++i) {
      const Section &sec = secs[i];
      _sections.push_back(sec);

      cout << sec.name << endl;
    }
  }

};

struct DotNetFileParser {
private:
  std::string _filePath;
  struct stat _fileInfo;
  void *_fileData {nullptr};

public:
  DotNetFileParser(std::string &filePath): _filePath(filePath) {}

  void Parse() {
    int fd = open(_filePath.c_str(), O_RDONLY);
    fstat(fd, &_fileInfo);
    _fileData = mmap(NULL, _fileInfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    PE pe(reinterpret_cast<const MZHeader *>(_fileData));

    const DataDirectory &clrDir = pe.GetDataDirectory(DataCLRHeader);
    const Section &clrSec = pe.GetSectionByDirectory(clrDir);

    cout << clrSec.name << endl;

    const CLRHeader *clrHeader = pe.MapRVA<const CLRHeader *>(clrDir);
    const MetadataHeader *metadataHeader = pe.MapRVA<const MetadataHeader *>(clrHeader->MetaDataDirectoryAddress);

    MetadataParser metadataParser(metadataHeader);
    Metadata metadata = metadataParser.Parse();
    Models::ModelBuilder builer(metadata);
    builer.Build();
  }

  ~DotNetFileParser() {
    if (_fileData != nullptr) {
      munmap(_fileData, _fileInfo.st_size);
      _fileData = nullptr;
    }
  }
};


int main() {
  std::string filePath = "/Users/admin/Desktop/TextAsset/MainScriptsHotUpdate.dll";
  DotNetFileParser parser(filePath);
  parser.Parse();
  return 0;
}
