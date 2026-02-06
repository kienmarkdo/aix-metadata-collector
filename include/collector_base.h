/**
 * @file collector_base.h
 * @brief Abstract base class for all metadata collectors
 *
 * This file defines the interface that all metadata collectors must implement.
 * The design follows the Strategy pattern, allowing different collectors
 * (process, file, port) to be used interchangeably.
 */

#ifndef AIX_METADATA_COLLECTOR_BASE_H
#define AIX_METADATA_COLLECTOR_BASE_H

#include "types.h"

namespace AixMetadata {

/**
 * @brief Abstract base class for metadata collectors
 *
 * All specific collectors (ProcessCollector, FileCollector, PortCollector)
 * inherit from this base class and implement the collect() method.
 */
class CollectorBase {
public:
    virtual ~CollectorBase() = default;

    /**
     * @brief Collect metadata for a given identifier
     *
     * @param identifier The identifier to query (PID string, file path, or port number)
     * @return MetadataResult containing all collected metadata
     */
    virtual MetadataResult collect(const std::string& identifier) = 0;

    /**
     * @brief Get the type of this collector
     * @return QueryType enum indicating what type of collector this is
     */
    virtual QueryType getType() const = 0;

    /**
     * @brief Get a human-readable name for this collector
     * @return String name of the collector
     */
    virtual std::string getName() const = 0;

protected:
    /**
     * @brief Helper to create an error result
     * @param identifier The identifier that was queried
     * @param errorMsg The error message
     * @return MetadataResult with success=false and the error message
     */
    MetadataResult createErrorResult(const std::string& identifier,
                                     const std::string& errorMsg) {
        MetadataResult result;
        result.success = false;
        result.identifier = identifier;
        result.errorMessage = errorMsg;
        return result;
    }
};

} // namespace AixMetadata

#endif // AIX_METADATA_COLLECTOR_BASE_H
