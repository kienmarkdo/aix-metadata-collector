/**
 * @file port_collector.h
 * @brief Network port metadata collector for AIX
 *
 * This collector retrieves metadata for network connections on a given port.
 * On AIX, network connection information can be obtained via:
 *   - netstat command parsing (portable approach)
 *   - /proc/net filesystem (limited on AIX compared to Linux)
 *   - libperfstat for some network statistics
 *   - getkerninfo() for socket table access
 *
 * For this PoC, we use netstat parsing as it's the most reliable
 * and portable approach on AIX 7.2.
 */

#ifndef AIX_METADATA_PORT_COLLECTOR_H
#define AIX_METADATA_PORT_COLLECTOR_H

#include "collector_base.h"
#include <vector>

namespace AixMetadata {

/**
 * @brief Information about a single network connection
 */
struct ConnectionInfo {
    std::string protocol;        ///< "tcp" or "udp"
    std::string localAddress;    ///< Local IP address
    std::string localPort;       ///< Local port number
    std::string remoteAddress;   ///< Remote IP address (for TCP)
    std::string remotePort;      ///< Remote port number (for TCP)
    std::string state;           ///< Connection state (LISTEN, ESTABLISHED, etc.)
    pid_t pid;                   ///< Process ID (if available)
    std::string processName;     ///< Process name (if available)
    std::string user;            ///< User owning the socket (if available)
};

/**
 * @brief Collects metadata for network ports on AIX
 *
 * Given a port number, this collector retrieves:
 *   - All connections using that port (listening or connected)
 *   - Protocol (TCP/UDP)
 *   - Local and remote addresses
 *   - Connection state
 *   - Process ID and name using the port
 *   - User owning the process
 *   - Whether it's IPv4 or IPv6
 */
class PortCollector : public CollectorBase {
public:
    /**
     * @brief Constructor
     * @param proto Protocol filter (default: both TCP and UDP)
     */
    explicit PortCollector(Protocol proto = Protocol::Both);
    ~PortCollector() override = default;

    /**
     * @brief Collect all available metadata for a port
     *
     * @param identifier The port number as a string (e.g., "22", "80")
     * @return MetadataResult containing all port/connection metadata
     */
    MetadataResult collect(const std::string& identifier) override;

    QueryType getType() const override { return QueryType::Port; }
    std::string getName() const override { return "PortCollector"; }

    /**
     * @brief Set the protocol filter
     * @param proto Protocol to filter by
     */
    void setProtocol(Protocol proto) { m_protocol = proto; }

private:
    Protocol m_protocol;  ///< Protocol filter

    /**
     * @brief Parse port number from string
     * @param identifier Port as string
     * @param port Output: parsed port number
     * @return true if parsing succeeded
     */
    bool parsePort(const std::string& identifier, uint16_t& port);

    /**
     * @brief Collect TCP connections for a port using netstat
     * @param port Port number to query
     * @param connections Output: vector of connection info
     * @return true if successful
     */
    bool collectTcpConnections(uint16_t port, std::vector<ConnectionInfo>& connections);

    /**
     * @brief Collect UDP connections for a port using netstat
     * @param port Port number to query
     * @param connections Output: vector of connection info
     * @return true if successful
     */
    bool collectUdpConnections(uint16_t port, std::vector<ConnectionInfo>& connections);

    /**
     * @brief Parse netstat output to extract connection info
     * @param output Output from netstat command
     * @param port Port number we're looking for
     * @param protocol Protocol being parsed ("tcp" or "udp")
     * @param connections Output: vector of connection info
     */
    void parseNetstatOutput(const std::string& output,
                            uint16_t port,
                            const std::string& protocol,
                            std::vector<ConnectionInfo>& connections);

    /**
     * @brief Execute a command and capture its output
     * @param cmd Command to execute
     * @param output Output: captured stdout
     * @return true if command succeeded
     */
    bool executeCommand(const std::string& cmd, std::string& output);

    /**
     * @brief Find process info for a given socket/port
     * @param port Port number
     * @param protocol Protocol
     * @param info Output: connection info to update with PID/process name
     */
    void findProcessForPort(uint16_t port,
                            const std::string& protocol,
                            ConnectionInfo& info);
};

} // namespace AixMetadata

#endif // AIX_METADATA_PORT_COLLECTOR_H
