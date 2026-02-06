/**
 * @file types.cpp
 * @brief Implementation of common types
 */

#include "types.h"
#include <sstream>

namespace AixMetadata {

void MetadataResult::addAttribute(const std::string& name, int64_t value) {
    std::ostringstream oss;
    oss << value;
    addAttribute(name, oss.str());
}

void MetadataResult::addAttribute(const std::string& name, uint64_t value) {
    std::ostringstream oss;
    oss << value;
    addAttribute(name, oss.str());
}

} // namespace AixMetadata
