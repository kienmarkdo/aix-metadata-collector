/**
 * @file process_collector.h
 * @brief Process metadata collector for AIX
 *
 * This collector retrieves comprehensive metadata for a process given its PID.
 * It uses AIX-specific APIs including:
 *   - /proc filesystem for basic process info
 *   - getprocs64() for detailed process structure
 *   - procfiles64() for open file descriptors
 *   - AIX-specific structures from <procinfo.h>
 */

#ifndef AIX_METADATA_PROCESS_COLLECTOR_H
#define AIX_METADATA_PROCESS_COLLECTOR_H

#include "collector_base.h"
#include <sys/types.h>

namespace AixMetadata {

/**
 * @brief Collects metadata for a process on AIX
 *
 * Given a PID, this collector retrieves:
 *   - Process name and command line arguments
 *   - User ID (UID) and Group ID (GID)
 *   - Parent Process ID (PPID)
 *   - Process state and priority
 *   - Memory usage (virtual, resident)
 *   - CPU usage statistics
 *   - Start time
 *   - Executable path
 *   - Current working directory
 *   - Open file descriptors
 *   - Environment variables (if accessible)
 */
class ProcessCollector : public CollectorBase {
public:
    ProcessCollector() = default;
    ~ProcessCollector() override = default;

    /**
     * @brief Collect all available metadata for a process
     *
     * @param identifier The PID as a string (e.g., "1234")
     * @return MetadataResult containing all process metadata
     */
    MetadataResult collect(const std::string& identifier) override;

    QueryType getType() const override { return QueryType::Process; }
    std::string getName() const override { return "ProcessCollector"; }

private:
    /**
     * @brief Parse PID from string identifier
     * @param identifier PID as string
     * @param pid Output: parsed PID
     * @return true if parsing succeeded
     */
    bool parsePid(const std::string& identifier, pid_t& pid);

    /**
     * @brief Collect basic process info using getprocs64()
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     * @return true if successful
     */
    bool collectBasicInfo(pid_t pid, MetadataResult& result);

    /**
     * @brief Collect command line arguments from /proc
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     */
    void collectCommandLine(pid_t pid, MetadataResult& result);

    /**
     * @brief Collect environment variables from /proc
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     */
    void collectEnvironment(pid_t pid, MetadataResult& result);

    /**
     * @brief Collect open file descriptors
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     */
    void collectOpenFiles(pid_t pid, MetadataResult& result);

    /**
     * @brief Collect executable path
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     */
    void collectExecutablePath(pid_t pid, MetadataResult& result);

    /**
     * @brief Collect current working directory
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     */
    void collectWorkingDirectory(pid_t pid, MetadataResult& result);

    /**
     * @brief Collect credential information (effective/real UID/GID)
     * @param pid Process ID
     * @param result Output: MetadataResult to populate
     */
    void collectCredentials(pid_t pid, MetadataResult& result);

    /**
     * @brief Convert process state code to human-readable string
     * @param state AIX process state code
     * @return Human-readable state string
     */
    std::string stateToString(unsigned char state);

    /**
     * @brief Convert time value to ISO 8601 string
     * @param timeVal Time value (seconds since epoch or AIX time format)
     * @return ISO 8601 formatted string
     */
    std::string timeToString(uint64_t timeVal);

    /**
     * @brief Collect WPAR (Workload Partition) information for a process
     *
     * Determines if the process is running inside a WPAR container by
     * reading the pi_cid (Corral ID) field from the procentry64 structure.
     *
     * From the Global environment, this can identify which WPAR a process
     * belongs to:
     *   - pi_cid == 0: Process is in Global environment
     *   - pi_cid > 0:  Process is in a WPAR with that Corral ID
     *
     * The WPAR name is resolved by looking up the CID in /etc/corrals/index.
     *
     * @param pid Process ID
     * @param result Output: MetadataResult to populate with WPAR info
     */
    void collectWparInfo(pid_t pid, MetadataResult& result);
};

} // namespace AixMetadata

#endif // AIX_METADATA_PROCESS_COLLECTOR_H
