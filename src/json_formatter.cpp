/**
 * @file json_formatter.cpp
 * @brief Implementation of JSON output formatter
 *
 * This file implements a simple JSON formatter that doesn't require
 * external libraries. It properly escapes special characters and
 * supports both compact and pretty-printed output.
 */

#include "json_formatter.h"

#include <sstream>
#include <iomanip>

namespace AixMetadata {

std::string JsonFormatter::format(const MetadataResult& result, bool prettyPrint) {
    std::ostringstream json;
    std::string nl = prettyPrint ? "\n" : "";
    std::string sp = prettyPrint ? " " : "";

    json << "{" << nl;

    // success field
    json << indent(prettyPrint ? 1 : 0)
         << "\"success\":" << sp << (result.success ? "true" : "false") << "," << nl;

    // type field
    json << indent(prettyPrint ? 1 : 0)
         << "\"type\":" << sp << "\"" << escapeString(result.type) << "\"," << nl;

    // identifier field
    json << indent(prettyPrint ? 1 : 0)
         << "\"identifier\":" << sp << "\"" << escapeString(result.identifier) << "\"," << nl;

    // error message (if any)
    if (!result.success && !result.errorMessage.empty()) {
        json << indent(prettyPrint ? 1 : 0)
             << "\"error\":" << sp << "\"" << escapeString(result.errorMessage) << "\"," << nl;
    }

    // attributes
    json << indent(prettyPrint ? 1 : 0) << "\"attributes\":" << sp << "{" << nl;

    bool firstAttr = true;
    for (const auto& attr : result.attributes) {
        if (!firstAttr) {
            json << "," << nl;
        }
        firstAttr = false;

        json << formatAttribute(attr, prettyPrint, 2);
    }

    json << nl << indent(prettyPrint ? 1 : 0) << "}" << nl;
    json << "}";

    return json.str();
}

std::string JsonFormatter::formatArray(const std::vector<MetadataResult>& results,
                                        bool prettyPrint) {
    std::ostringstream json;
    std::string nl = prettyPrint ? "\n" : "";

    json << "[" << nl;

    bool first = true;
    for (const auto& result : results) {
        if (!first) {
            json << "," << nl;
        }
        first = false;

        // Indent each result object
        std::string resultJson = format(result, prettyPrint);

        if (prettyPrint) {
            // Add indentation to each line
            std::istringstream stream(resultJson);
            std::string line;
            bool firstLine = true;
            while (std::getline(stream, line)) {
                if (!firstLine) {
                    json << "\n";
                }
                json << indent(1) << line;
                firstLine = false;
            }
        } else {
            json << resultJson;
        }
    }

    json << nl << "]";

    return json.str();
}

std::string JsonFormatter::escapeString(const std::string& str) {
    std::ostringstream escaped;

    for (char c : str) {
        switch (c) {
            case '"':
                escaped << "\\\"";
                break;
            case '\\':
                escaped << "\\\\";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                // Handle control characters
                if (static_cast<unsigned char>(c) < 0x20) {
                    escaped << "\\u" << std::hex << std::setfill('0')
                            << std::setw(4) << static_cast<int>(c);
                } else {
                    escaped << c;
                }
                break;
        }
    }

    return escaped.str();
}

std::string JsonFormatter::indent(int level) {
    return std::string(level * 2, ' ');
}

std::string JsonFormatter::formatAttribute(const MetadataAttribute& attr,
                                            bool prettyPrint,
                                            int indentLevel) {
    std::ostringstream json;
    std::string sp = prettyPrint ? " " : "";

    json << indent(prettyPrint ? indentLevel : 0)
         << "\"" << escapeString(attr.name) << "\":" << sp;

    if (attr.values.size() == 1) {
        // Single value - output as string
        json << "\"" << escapeString(attr.values[0]) << "\"";
    } else if (attr.values.empty()) {
        // No values - output null
        json << "null";
    } else {
        // Multiple values - output as array
        json << "[";

        bool first = true;
        for (const auto& value : attr.values) {
            if (!first) {
                json << "," << sp;
            }
            first = false;
            json << "\"" << escapeString(value) << "\"";
        }

        json << "]";
    }

    return json.str();
}

} // namespace AixMetadata
