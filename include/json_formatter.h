/**
 * @file json_formatter.h
 * @brief JSON output formatter for metadata results
 *
 * This file provides a simple JSON formatter that doesn't require
 * external JSON libraries. It converts MetadataResult objects to
 * JSON strings suitable for output or further processing.
 */

#ifndef AIX_METADATA_JSON_FORMATTER_H
#define AIX_METADATA_JSON_FORMATTER_H

#include "types.h"
#include <string>
#include <sstream>

namespace AixMetadata {

/**
 * @brief Formats MetadataResult objects as JSON
 *
 * This class provides static methods to convert metadata results
 * to properly formatted JSON strings. It handles escaping of
 * special characters and supports both compact and pretty-printed output.
 */
class JsonFormatter {
public:
    /**
     * @brief Convert a MetadataResult to JSON string
     *
     * @param result The metadata result to format
     * @param prettyPrint Whether to format with indentation (default: true)
     * @return JSON string representation
     */
    static std::string format(const MetadataResult& result, bool prettyPrint = true);

    /**
     * @brief Convert multiple MetadataResults to JSON array string
     *
     * @param results Vector of metadata results
     * @param prettyPrint Whether to format with indentation
     * @return JSON array string representation
     */
    static std::string formatArray(const std::vector<MetadataResult>& results,
                                   bool prettyPrint = true);

private:
    /**
     * @brief Escape special characters in a string for JSON
     * @param str Input string
     * @return Escaped string safe for JSON
     */
    static std::string escapeString(const std::string& str);

    /**
     * @brief Get indentation string for pretty printing
     * @param level Indentation level
     * @return String of spaces for indentation
     */
    static std::string indent(int level);

    /**
     * @brief Format a single attribute as JSON
     * @param attr The attribute to format
     * @param prettyPrint Whether to format with indentation
     * @param indentLevel Current indentation level
     * @return JSON string for the attribute
     */
    static std::string formatAttribute(const MetadataAttribute& attr,
                                       bool prettyPrint,
                                       int indentLevel);
};

} // namespace AixMetadata

#endif // AIX_METADATA_JSON_FORMATTER_H
