/**
 * @file types.h
 * @brief Common type definitions for the AIX Metadata Collector
 *
 * This file defines common data structures used throughout the metadata
 * collector, including the MetadataResult structure that holds key-value
 * pairs of collected metadata.
 */

#ifndef AIX_METADATA_TYPES_H
#define AIX_METADATA_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace AixMetadata {

/**
 * @brief Represents a single metadata attribute (key-value pair)
 *
 * Metadata attributes can have multiple values for the same key,
 * which is useful for attributes like "open_file_descriptors" that
 * may have multiple entries.
 */
struct MetadataAttribute {
    std::string name;                  ///< Attribute name (e.g., "uid", "path", "port")
    std::vector<std::string> values;   ///< One or more values for this attribute

    MetadataAttribute() = default;

    MetadataAttribute(const std::string& n, const std::string& v)
        : name(n) {
        values.push_back(v);
    }

    MetadataAttribute(const std::string& n, const std::vector<std::string>& v)
        : name(n), values(v) {}
};

/**
 * @brief Represents the result of a metadata collection operation
 *
 * This is the main data structure returned by all collectors.
 * It contains the type of metadata (process, file, port), the
 * identifier used to query it, and all collected attributes.
 */
struct MetadataResult {
    std::string type;                              ///< Type: "process", "file", or "port"
    std::string identifier;                        ///< The PID, file path, or port number
    std::vector<MetadataAttribute> attributes;     ///< Collected metadata attributes
    bool success;                                  ///< Whether the collection succeeded
    std::string errorMessage;                      ///< Error message if success is false

    MetadataResult() : success(false) {}

    /**
     * @brief Add a single-value attribute
     * @param name Attribute name
     * @param value Attribute value
     */
    void addAttribute(const std::string& name, const std::string& value) {
        attributes.emplace_back(name, value);
    }

    /**
     * @brief Add a multi-value attribute
     * @param name Attribute name
     * @param values Vector of attribute values
     */
    void addAttribute(const std::string& name, const std::vector<std::string>& values) {
        attributes.emplace_back(name, values);
    }

    /**
     * @brief Add an integer attribute (converted to string)
     * @param name Attribute name
     * @param value Integer value
     */
    void addAttribute(const std::string& name, int64_t value);

    /**
     * @brief Add an unsigned integer attribute (converted to string)
     * @param name Attribute name
     * @param value Unsigned integer value
     */
    void addAttribute(const std::string& name, uint64_t value);
};

/**
 * @brief Enumeration of metadata query types
 */
enum class QueryType {
    Process,    ///< Query by Process ID (PID)
    File,       ///< Query by file path
    Port        ///< Query by port number
};

/**
 * @brief Protocol type for port queries
 */
enum class Protocol {
    TCP,        ///< TCP protocol
    UDP,        ///< UDP protocol
    Both        ///< Query both TCP and UDP
};

} // namespace AixMetadata

#endif // AIX_METADATA_TYPES_H
