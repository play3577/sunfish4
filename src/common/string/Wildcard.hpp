/* Wildcard.h
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_COMMON_WILDCARD_WILDCARD_HPP__
#define SUNFISH_COMMON_WILDCARD_WILDCARD_HPP__

#include <string>

namespace sunfish {

class Wildcard {
public:
  Wildcard(const char* pattern) : pattern_(normalize(pattern)) {
  }
  Wildcard(const std::string& pattern) : pattern_(normalize(pattern)) {
  }

  bool match(const char* str) const;
  bool match(const std::string& str) const {
    return match(str.c_str());
  }

private:
  static std::string normalize(const char* pattern);
  static std::string normalize(const std::string& pattern) {
    return normalize(pattern.c_str());
  }

  static bool match(const char* pp, const char* pt);

private:
  std::string pattern_;

};

} // namespace sunfish

#endif // SUNFISH_COMMON_WILDCARD_WILDCARD_HPP__
