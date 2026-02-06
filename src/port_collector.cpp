/**
 * @file port_collector.cpp
 * @brief Implementation of network port metadata collector for AIX
 *
 * This file implements the PortCollector class which retrieves network
 * connection information for a given port number.
 *
 * On AIX, we use a combination of approaches:
 *   1. Parse 'netstat -Aan' output for connection information
 *   2. Use 'rmsock' or 'lsof' to correlate sockets to processes (if available)
 *   3. Fall back to netstat -p for process information
 *
 * Note: Some operations may require root privileges for full process information.
 */

#include "port_collector.h"

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace AixMetadata {

PortCollector::PortCollector(Protocol proto)
    : m_protocol(proto) {
}

MetadataResult PortCollector::collect(const std::string& identifier) {
    MetadataResult result;
    result.type = "port";
    result.identifier = identifier;

    uint16_t port;
    if (!parsePort(identifier, port)) {
        return createErrorResult(identifier, "Invalid port number: " + identifier);
    }

    std::vector<ConnectionInfo> connections;

    // Collect TCP connections if requested
    if (m_protocol == Protocol::TCP || m_protocol == Protocol::Both) {
        collectTcpConnections(port, connections);
    }

    // Collect UDP connections if requested
    if (m_protocol == Protocol::UDP || m_protocol == Protocol::Both) {
        collectUdpConnections(port, connections);
    }

    if (connections.empty()) {
        result.success = true;
        result.addAttribute("status", "no_connections_found");
        result.addAttribute("port", identifier);
        return result;
    }

    result.success = true;
    result.addAttribute("port", identifier);
    result.addAttribute("num_connections", static_cast<int64_t>(connections.size()));

    // Add each connection as attributes
    int connIndex = 0;
    for (const auto& conn : connections) {
        std::ostringstream prefix;
        prefix << "connection_" << connIndex << "_";
        std::string pfx = prefix.str();

        result.addAttribute(pfx + "protocol", conn.protocol);
        result.addAttribute(pfx + "local_address", conn.localAddress);
        result.addAttribute(pfx + "local_port", conn.localPort);
        result.addAttribute(pfx + "remote_address", conn.remoteAddress);
        result.addAttribute(pfx + "remote_port", conn.remotePort);
        result.addAttribute(pfx + "state", conn.state);

        if (conn.pid > 0) {
            std::ostringstream pidStr;
            pidStr << conn.pid;
            result.addAttribute(pfx + "pid", pidStr.str());
        }

        if (!conn.processName.empty()) {
            result.addAttribute(pfx + "process", conn.processName);
        }

        if (!conn.user.empty()) {
            result.addAttribute(pfx + "user", conn.user);
        }

        connIndex++;
    }

    return result;
}

bool PortCollector::parsePort(const std::string& identifier, uint16_t& port) {
    char* endPtr = nullptr;
    long value = std::strtol(identifier.c_str(), &endPtr, 10);

    // Check for conversion errors
    if (endPtr == identifier.c_str() || *endPtr != '\0') {
        return false;
    }

    // Check for valid port range (1-65535)
    if (value <= 0 || value > 65535) {
        return false;
    }

    port = static_cast<uint16_t>(value);
    return true;
}

bool PortCollector::executeCommand(const std::string& cmd, std::string& output) {
    // Use popen to execute command and capture output
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return false;
    }

    char buffer[4096];
    output.clear();

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    int status = pclose(pipe);
    return (status == 0 || !output.empty());
}

bool PortCollector::collectTcpConnections(uint16_t port, std::vector<ConnectionInfo>& connections) {
    std::string output;

    // On AIX, use netstat -Aan for TCP connections
    // -A: show socket address (for process correlation)
    // -a: show all sockets
    // -n: numeric addresses
    std::string cmd = "netstat -Aan -f inet 2>/dev/null | grep tcp";

    if (!executeCommand(cmd, output)) {
        // Try alternative command without -A
        cmd = "netstat -an -f inet 2>/dev/null | grep tcp";
        if (!executeCommand(cmd, output)) {
            return false;
        }
    }

    parseNetstatOutput(output, port, "tcp", connections);

    // Also check IPv6
    cmd = "netstat -Aan -f inet6 2>/dev/null | grep tcp";
    if (executeCommand(cmd, output)) {
        parseNetstatOutput(output, port, "tcp6", connections);
    }

    return true;
}

bool PortCollector::collectUdpConnections(uint16_t port, std::vector<ConnectionInfo>& connections) {
    std::string output;

    // On AIX, use netstat for UDP
    std::string cmd = "netstat -Aan -f inet 2>/dev/null | grep udp";

    if (!executeCommand(cmd, output)) {
        cmd = "netstat -an -f inet 2>/dev/null | grep udp";
        if (!executeCommand(cmd, output)) {
            return false;
        }
    }

    parseNetstatOutput(output, port, "udp", connections);

    // Also check IPv6
    cmd = "netstat -Aan -f inet6 2>/dev/null | grep udp";
    if (executeCommand(cmd, output)) {
        parseNetstatOutput(output, port, "udp6", connections);
    }

    return true;
}

void PortCollector::parseNetstatOutput(const std::string& output,
                                        uint16_t port,
                                        const std::string& protocol,
                                        std::vector<ConnectionInfo>& connections) {
    /*
     * AIX netstat -Aan output format example:
     * f1000e0001891398 tcp4       0      0  *.22               *.*                LISTEN
     * f1000e000189bb98 tcp4       0      0  192.168.1.1.22     192.168.1.2.54321  ESTABLISHED
     *
     * Format: socket_addr proto recv-q send-q local_addr foreign_addr state
     *
     * Without -A:
     * tcp4       0      0  *.22               *.*                LISTEN
     */

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Tokenize the line
        std::vector<std::string> tokens;
        std::istringstream lineStream(line);
        std::string token;

        while (lineStream >> token) {
            tokens.push_back(token);
        }

        // We need at least 6 tokens for connection info
        // With -A: socket proto recvq sendq local foreign state
        // Without -A: proto recvq sendq local foreign state
        if (tokens.size() < 6) continue;

        // Determine if first token is socket address (hex) or protocol
        size_t offset = 0;
        std::string socketAddr;

        // Check if first token looks like a hex address
        if (tokens[0].length() > 10 && tokens[0].find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
            socketAddr = tokens[0];
            offset = 1;
        }

        if (tokens.size() < offset + 6) continue;

        std::string localAddr = tokens[offset + 3];
        std::string foreignAddr = tokens[offset + 4];
        std::string state = tokens[offset + 5];

        // Extract port from local address (format: addr.port or *.port)
        size_t lastDot = localAddr.rfind('.');
        if (lastDot == std::string::npos) continue;

        std::string localPortStr = localAddr.substr(lastDot + 1);
        std::string localIp = localAddr.substr(0, lastDot);

        // Check if this matches our target port
        char* endPtr;
        long localPortNum = std::strtol(localPortStr.c_str(), &endPtr, 10);
        if (*endPtr != '\0' || localPortNum != port) {
            // Also check foreign port for established connections
            size_t foreignLastDot = foreignAddr.rfind('.');
            if (foreignLastDot != std::string::npos) {
                std::string foreignPortStr = foreignAddr.substr(foreignLastDot + 1);
                long foreignPortNum = std::strtol(foreignPortStr.c_str(), &endPtr, 10);
                if (*endPtr != '\0' || foreignPortNum != port) {
                    continue;  // Neither local nor foreign port matches
                }
            } else {
                continue;
            }
        }

        // This connection matches our port
        ConnectionInfo info;
        info.protocol = protocol;
        info.localAddress = localIp;
        info.localPort = localPortStr;
        info.state = state;
        info.pid = 0;

        // Parse foreign address
        size_t foreignLastDot = foreignAddr.rfind('.');
        if (foreignLastDot != std::string::npos) {
            info.remoteAddress = foreignAddr.substr(0, foreignLastDot);
            info.remotePort = foreignAddr.substr(foreignLastDot + 1);
        } else {
            info.remoteAddress = foreignAddr;
            info.remotePort = "*";
        }

        // Try to find process info using the socket address
        if (!socketAddr.empty()) {
            findProcessForPort(port, protocol, info);
        }

        connections.push_back(info);
    }
}

void PortCollector::findProcessForPort(uint16_t port,
                                        const std::string& protocol,
                                        ConnectionInfo& info) {
#ifdef _AIX
    /*
     * On AIX, we can use 'rmsock' to find the process holding a socket.
     * However, rmsock is primarily for releasing sockets, not querying.
     *
     * Alternative approaches:
     * 1. Use 'lsof -i :port' if lsof is installed
     * 2. Use 'procfiles -n' to search through all processes
     * 3. Parse /proc filesystem
     *
     * For this PoC, we'll try lsof first as it's commonly installed.
     */

    std::ostringstream cmd;
    cmd << "lsof -i :" << port << " -n -P 2>/dev/null | grep -i " << protocol;

    std::string output;
    if (!executeCommand(cmd.str(), output) || output.empty()) {
        // lsof not available or no results
        // Try alternative: use procfiles on all processes (expensive)
        return;
    }

    /*
     * lsof output format:
     * COMMAND   PID USER   FD   TYPE  DEVICE SIZE/OFF NODE NAME
     * sshd     1234 root    3u  IPv4   12345      0t0  TCP *:22 (LISTEN)
     */

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::vector<std::string> tokens;
        std::istringstream lineStream(line);
        std::string token;

        while (lineStream >> token) {
            tokens.push_back(token);
        }

        // Skip header line
        if (tokens.size() < 2 || tokens[0] == "COMMAND") continue;

        if (tokens.size() >= 3) {
            info.processName = tokens[0];

            char* endPtr;
            long pid = std::strtol(tokens[1].c_str(), &endPtr, 10);
            if (*endPtr == '\0') {
                info.pid = static_cast<pid_t>(pid);
            }

            info.user = tokens[2];
            break;  // Use first match
        }
    }
#else
    // Non-AIX: try lsof on Linux/macOS
    std::ostringstream cmd;
    cmd << "lsof -i :" << port << " -n -P 2>/dev/null | grep -v COMMAND | head -1";

    std::string output;
    if (executeCommand(cmd.str(), output) && !output.empty()) {
        std::istringstream lineStream(output);
        std::string procName, pidStr, user;

        if (lineStream >> procName >> pidStr >> user) {
            info.processName = procName;
            info.user = user;

            char* endPtr;
            long pid = std::strtol(pidStr.c_str(), &endPtr, 10);
            if (*endPtr == '\0') {
                info.pid = static_cast<pid_t>(pid);
            }
        }
    }
#endif
}

} // namespace AixMetadata
