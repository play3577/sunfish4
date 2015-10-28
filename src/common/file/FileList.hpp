/* fileList.h
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_COMMON_FIL_FILELIST_HPP__
#define SUNFISH_COMMON_FIL_FILELIST_HPP__

#include "common/Def.hpp"

#include <list>
#include <string>

namespace sunfish {

class FileList{
public:
  using iterator = std::list<std::string>::iterator;

private:
  std::list<std::string> flist_;

public:
  FileList() = default;
  FileList(const FileList&) = default;
  FileList(FileList&& src) NOEXCEPT = default;
  ~FileList() = default;

  size_t enumerate(const char* directory, const char* extension);

  void clear(){ flist_.clear(); }
  size_t size() { return flist_.size(); }

  iterator begin() { return flist_.begin(); }
  iterator end() { return flist_.end(); }

};

} // namespace sunfish

#endif // SUNFISH_COMMON_FIL_FILELIST_HPP__
