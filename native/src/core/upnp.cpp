#include "upnp.h"

#include <QUdpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QXmlStreamReader>
#include <QUrl>
#include <QUrlQuery>
#include <QHostInfo>

namespace UPnP {

namespace {

constexpr const char *kSsdpAddr = "239.255.255.250";
constexpr quint16 kSsdpPort = 1900;

struct ControlEndpoint {
    QString url;       // absolute control URL (query-free)
    QString serviceId; // urn:schemas-upnp-org:service:WAN(IP|PPP)Connection:1
};

// Blocking HTTP helper (the discovery flow is short-lived).
QByteArray httpBlocking(const QNetworkRequest &req, const QByteArray &verb,
                        const QByteArray &body, bool *ok, int timeoutMs = 8000)
{
    QNetworkAccessManager nam;
    QNetworkRequest copy = req;
    copy.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = verb == "GET"
        ? nam.get(copy)
        : nam.sendCustomRequest(copy, verb, body);
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (ok) *ok = (reply->error() == QNetworkReply::NoError);
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

// SSDP M-SEARCH for InternetGatewayDevice; returns the device description URL.
QString discoverDeviceDescriptionUrl(int timeoutMs)
{
    QUdpSocket socket;
    const QByteArray msg =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    socket.writeDatagram(msg, QHostAddress(kSsdpAddr), kSsdpPort);
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QUdpSocket::readyRead, &loop, &QEventLoop::quit);
    loop.exec();
    while (socket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(socket.pendingDatagramSize()));
        socket.readDatagram(datagram.data(), datagram.size());
        for (const QByteArray &line : datagram.split('\n')) {
            const QString l = QString::fromLatin1(line).trimmed();
            if (l.startsWith("LOCATION:", Qt::CaseInsensitive)) {
                return l.mid(9).trimmed();
            }
        }
    }
    return QString();
}

// Walks the device description XML for a WANIPConnection/WANPPPConnection
// service, resolving the relative controlURL against the description URL.
ControlEndpoint findControlEndpoint(const QString &descUrlStr)
{
    ControlEndpoint ep;
    bool ok = false;
    const QByteArray xml = httpBlocking(QNetworkRequest(QUrl(descUrlStr)), "GET",
                                        QByteArray(), &ok);
    if (!ok) return ep;
    const QUrl descUrl(descUrlStr);
    QXmlStreamReader reader(xml);
    QString currentServiceType;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (reader.name() == "serviceType") {
                currentServiceType = reader.readElementText().trimmed();
            } else if (reader.name() == "controlURL"
                       && (currentServiceType.contains("WANIPConnection")
                           || currentServiceType.contains("WANPPPConnection"))) {
                ep.serviceId = currentServiceType;
                ep.url = descUrl.resolved(QUrl(reader.readElementText().trimmed())).toString();
                break;
            }
        }
    }
    return ep;
}

QByteArray soapEnvelope(const QString &action, const QString &serviceType,
                        const QString &protocol, int port, const QString &client)
{
    return QString(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:%1 xmlns:u=\"%2\">"
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%3</NewExternalPort>"
        "<NewProtocol>%4</NewProtocol>"
        "<NewInternalPort>%5</NewInternalPort>"
        "<NewInternalClient>%6</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>Novetus</NewPortMappingDescription>"
        "<NewLeaseDuration>0</NewLeaseDuration>"
        "</u:%1>"
        "</s:Body>"
        "</s:Envelope>")
        .arg(action, serviceType, QString::number(port), protocol,
             QString::number(port), client)
        .toUtf8();
}

// Sends a SOAP action and returns the response body. ok=false on transport
// failure; errorCodes collects UPnP error codes from the SOAP fault.
QByteArray soapCall(const ControlEndpoint &ep, const QString &action,
                    const QByteArray &body, bool *ok, QStringList *errorCodes = nullptr)
{
    QNetworkRequest req(QUrl(ep.url));
    req.setRawHeader("SOAPAction", "\"" + ep.serviceId.toUtf8() + "#" + action.toUtf8() + "\"");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    QStringList dummyCodes;
    QByteArray resp = httpBlocking(req, "POST", body, ok);
    if (*ok && QString::fromUtf8(resp).contains("<u:" + action + "Response")) {
        return resp;
    }
    // A SOAP fault carries the UPnP error code; surface it to the caller.
    if (!errorCodes) errorCodes = &dummyCodes;
    QXmlStreamReader reader(resp);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.name() == "UPnPError" && reader.isStartElement()) {
            while (!reader.atEnd() && !(reader.name() == "UPnPError" && reader.isEndElement())) {
                reader.readNext();
                if (reader.name() == "errorCode") {
                    *errorCodes << reader.readElementText().trimmed();
                }
            }
        }
    }
    if (errorCodes->isEmpty()) *ok = false;
    return resp;
}

QString localIp()
{
    const QList<QHostAddress> addrs = QHostInfo::fromName(QHostInfo::localHostName()).addresses();
    for (const QHostAddress &a : addrs) {
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback()) {
            return a.toString();
        }
    }
    return "192.168.1.1";
}

QString externalIpText(const ControlEndpoint &ep, const QString &alternateServerIp)
{
    if (!alternateServerIp.isEmpty()) {
        return alternateServerIp; // StartUPnP parity: AlternateServerIP wins
    }
    bool ok = false;
    const QByteArray body =
        QString("<?xml version=\"1.0\"?>"
                "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                "<s:Body><u:GetExternalIPAddress xmlns:u=\"%1\"/></s:Body>"
                "</s:Envelope>").arg(ep.serviceId).toUtf8();
    const QByteArray resp = soapCall(ep, "GetExternalIPAddress", body, &ok);
    if (ok) {
        QXmlStreamReader reader(resp);
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.name() == "NewExternalIPAddress") {
                return reader.readElementText().trimmed();
            }
        }
    }
    return QString();
}

// GetSpecificMapping + CreatePortMap parity: try to add the mapping; a
// conflict error means the mapping already exists, which is success.
bool mapPort(const ControlEndpoint &ep, const QString &protocol, int port, QString *error)
{
    bool ok = false;
    QStringList errorCodes;
    soapCall(ep, "AddPortMapping",
             soapEnvelope("AddPortMapping", ep.serviceId, protocol, port, localIp()),
             &ok, &errorCodes);
    if (ok) return true;
    for (const QString &code : errorCodes) {
        // 718 ConflictInMappingEntry, 725 OnlyPermanentLeasesSupported
        if (code == "718" || code == "725") return true;
    }
    *error = "unable to open port mapping (" + protocol + ")"
             + (errorCodes.isEmpty() ? QString() : " - error " + errorCodes.join("/"));
    return false;
}

} // namespace

DiscoveryResult openPort(int port, const QString &alternateServerIp)
{
    DiscoveryResult r;
    const QString descUrl = discoverDeviceDescriptionUrl(4000);
    if (descUrl.isEmpty()) {
        r.error = "no UPnP gateway found";
        return r;
    }
    const ControlEndpoint ep = findControlEndpoint(descUrl);
    if (ep.url.isEmpty()) {
        r.error = "no WANIPConnection control URL";
        return r;
    }
    r.found = true;

    QString mapError;
    const bool udpOk = mapPort(ep, "UDP", port, &mapError);
    const bool tcpOk = udpOk ? mapPort(ep, "TCP", port, &mapError) : false;
    if (!udpOk && !tcpOk) {
        r.error = mapError;
        return r;
    }
    r.externalIp = externalIpText(ep, alternateServerIp);
    return r;
}

DiscoveryResult closePort(int port, const QString &alternateServerIp)
{
    Q_UNUSED(alternateServerIp)
    DiscoveryResult r;
    const QString descUrl = discoverDeviceDescriptionUrl(4000);
    if (descUrl.isEmpty()) {
        r.error = "no UPnP gateway found";
        return r;
    }
    const ControlEndpoint ep = findControlEndpoint(descUrl);
    if (ep.url.isEmpty()) {
        r.error = "no WANIPConnection control URL";
        return r;
    }
    r.found = true;
    for (const char *proto : {"UDP", "TCP"}) {
        QStringList errorCodes;
        bool ok = false;
        soapCall(ep, "DeletePortMapping",
                 soapEnvelope("DeletePortMapping", ep.serviceId, proto, port, QString()),
                 &ok, &errorCodes);
        if (!ok && errorCodes.isEmpty()) r.error = "delete failed (" + QString(proto) + ")";
    }
    return r;
}

} // namespace UPnP
