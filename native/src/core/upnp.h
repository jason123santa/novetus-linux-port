#pragma once
#include <QString>

// Port of Novetus.Core.NetFuncs UPnP helpers (Mono.Nat replaced by a small
// SSDP discovery + SOAP IGD client over QtNetwork).
namespace UPnP {

struct DiscoveryResult {
    bool found = false;
    QString controlUrl; // absolute URL for the WANIPConnection control endpoint
    QString externalIp;
    QString error;
};

// DeviceFound/StartUPnP parity: discover the gateway, add the UDP and TCP
// mappings for `port` (only when no mapping exists yet, like the original),
// and report the external IP used for the console message.
DiscoveryResult openPort(int port, const QString &alternateServerIp = QString());

// StopUPnP parity: remove both mappings.
DiscoveryResult closePort(int port, const QString &alternateServerIp = QString());

} // namespace UPnP
