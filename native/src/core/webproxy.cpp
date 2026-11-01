#include "webproxy.h"
#include "config.h"
#include "extensions.h"
#include "paths.h"
#include "linuxcompat.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QRegularExpression>
#include <QDir>
#include <QProcess>

namespace WebProxy {

namespace {

constexpr int kProxyPort = 6171;

QTcpServer *server = nullptr;
Config *userConfig = nullptr;
Extensions::Manager *manager = nullptr;

// IsValidURL parity: hosts on roblox.com/robloxlabs.com with a Roblox or
// legacy Mozilla 4.0 user agent.
bool isValidHost(const QString &host)
{
    if (host.endsWith("robloxlabs.com")) return true;
    if (!host.endsWith("roblox.com")) return false;
    if (host.startsWith("www.") || host.startsWith("web.")
        || host.startsWith("assetgame.") || host.startsWith("wiki.")
        || host.endsWith("api.roblox.com") || host.startsWith("roblox.com")) {
        return true;
    }
    return false;
}

bool isValidUserAgent(const QString &ua)
{
    if (ua.isEmpty()) return false;
    const QString lower = ua.toLower();
    if (lower.contains("mozilla/4.0")) return true;
    return lower.contains("roblox");
}

// Sends a simple HTTP response (GenericResponse parity).
void sendResponse(QTcpSocket *socket, int status, const QByteArray &body = QByteArray())
{
    const char *text = "OK";
    switch (status) {
    case 404: text = "Not Found"; break;
    case 500: text = "Internal Server Error"; break;
    }
    QByteArray head;
    head += "HTTP/1.1 " + QByteArray::number(status) + " " + text + "\r\n";
    head += "Content-Type: text/plain\r\n";
    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    head += "Connection: close\r\n\r\n";
    socket->write(head + body);
    socket->flush();
    socket->disconnectFromHost();
}

// Parses a request head into method/path/host/headers. Returns false when the
// headers are incomplete (caller should wait for more data).
bool parseRequest(const QByteArray &data, QString &method, QString &path,
                  QString &host, QString &userAgent)
{
    const int end = data.indexOf("\r\n\r\n");
    if (end < 0) return false;
    const QStringList lines = QString::fromLatin1(data.left(end)).split("\r\n");
    if (lines.isEmpty()) return false;
    const QRegularExpression firstLine("^([A-Z]+)\\s+(\\S+)\\s+HTTP/\\d\\.\\d$");
    const QRegularExpressionMatch m = firstLine.match(lines[0]);
    if (!m.hasMatch()) return false;
    method = m.captured(1);
    path = m.captured(2);
    for (int i = 1; i < lines.size(); i++) {
        const int colon = lines[i].indexOf(':');
        if (colon <= 0) continue;
        const QString name = lines[i].left(colon).trimmed().toLower();
        const QString value = lines[i].mid(colon + 1).trimmed();
        if (name == "host") host = value;
        else if (name == "user-agent") userAgent = value;
    }
    return true;
}

class SocketHandler : public QObject
{
public:
    explicit SocketHandler(QTcpSocket *sock)
        : QObject(sock), m_sock(sock)
    {
        connect(sock, &QTcpSocket::readyRead, this, &SocketHandler::onReady);
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    }

private:
    void tunnel(QTcpSocket *socket, const QString &hostPort)
    {
        // Pass the CONNECT through without decryption (no MITM certs here).
        socket->write("HTTP/1.1 200 Connection Established\r\n\r\n");
        socket->flush();
        QTcpSocket *remote = new QTcpSocket(socket);
        remote->connectToHost(hostPort.section(':', 0, 0),
                              hostPort.contains(':') ? hostPort.section(':', 1, 1).toInt() : 443);
        connect(remote, &QTcpSocket::connected, socket, [remote, socket] {
            const QByteArray pending = socket->readAll();
            if (!pending.isEmpty()) remote->write(pending);
        });
        connect(socket, &QTcpSocket::readyRead, socket, [remote, socket] {
            remote->write(socket->readAll());
        });
        connect(remote, &QTcpSocket::readyRead, remote, [remote, socket] {
            socket->write(remote->readAll());
        });
        connect(remote, &QTcpSocket::disconnected, socket, &QTcpSocket::disconnectFromHost);
        connect(socket, &QTcpSocket::disconnected, remote, &QTcpSocket::disconnectFromHost);
    }

    void onReady()
    {
        const QByteArray data = m_sock->peek(m_sock->bytesAvailable());
        QString method, path, host, ua;
        if (!parseRequest(data, method, path, host, ua)) return; // wait for more

        if (method == "CONNECT") {
            m_sock->read(m_sock->bytesAvailable());
            tunnel(m_sock, host);
            return;
        }

        if (!isValidHost(host) || !isValidUserAgent(ua)) {
            forward(method, path, host);
            return;
        }

        // Valid Roblox host: extensions answer, otherwise 404 (OnRequest parity).
        m_sock->read(m_sock->bytesAvailable());
        Extensions::Response resp;
        if (!manager || !manager->onRequest(path, host, &resp)) {
            sendResponse(m_sock, 404);
            return;
        }
        if (!resp.valid) {
            sendResponse(m_sock, 500);
            return;
        }
        sendResponse(m_sock, resp.status, resp.body);
    }

    void forward(const QString &method, const QString &path, const QString &host)
    {
        // Minimal HTTP pass-through: open a second socket, replay the request,
        // then pipe the response back.
        m_sock->read(m_sock->bytesAvailable()); // discard; rebuild the request
        QTcpSocket *remote = new QTcpSocket(m_sock);
        remote->connectToHost(host, 80);
        connect(remote, &QTcpSocket::connected, remote, [this, remote, method, path, host] {
            QByteArray req;
            req += method.toLatin1() + " " + path.toLatin1() + " HTTP/1.1\r\n";
            req += "Host: " + host.toLatin1() + "\r\n";
            req += "User-Agent: Novetus-Native/1.0\r\n";
            req += "Connection: close\r\n\r\n";
            remote->write(req);
        });
        connect(remote, &QTcpSocket::readyRead, remote, [this, remote] {
            m_sock->write(remote->readAll());
            m_sock->flush();
        });
        connect(remote, &QTcpSocket::disconnected, m_sock, &QTcpSocket::disconnectFromHost);
        connect(m_sock, &QTcpSocket::disconnected, remote, &QTcpSocket::disconnectFromHost);
    }

    QTcpSocket *m_sock;
};

// Wine registry helpers: the Windows clients read IE's proxy settings from
// the registry, so route them through our local proxy like the original's
// SetAsSystemProxy / RestoreOriginalProxySettings.
bool runWineReg(const QStringList &args)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", LinuxCompat::winePrefix());
    p.setProcessEnvironment(env);
    p.start(LinuxCompat::wineCommandProgram(),
            QStringList(LinuxCompat::wineCommandPrefixArgs()) << args);
    return p.waitForFinished(30000) && p.exitCode() == 0;
}

bool setSystemProxy(bool on)
{
    const QString key = "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
    if (on) {
        const bool okEnable = runWineReg({"reg", "add", key, "/v", "ProxyEnable",
                                          "/t", "REG_DWORD", "/d", "1", "/f"});
        const bool okServer = runWineReg({"reg", "add", key, "/v", "ProxyServer",
                                          "/t", "REG_SZ", "/d", "127.0.0.1:6171", "/f"});
        return okEnable && okServer;
    }
    return runWineReg({"reg", "add", key, "/v", "ProxyEnable",
                       "/t", "REG_DWORD", "/d", "0", "/f"});
}

} // namespace

bool start(bool setSystemProxy_)
{
    if (running()) {
        return true;
    }
    if (!userConfig) {
        userConfig = new Config(QStringLiteral("Config"), Paths::configDir + "/config.json");
        userConfig->load();
    }
    if (!manager) {
        manager = new Extensions::Manager();
        manager->load(Paths::novetusExts + "/webproxy");
        manager->onProxyStart();
    }
    if (!server) {
        server = new QTcpServer();
    }
    QObject::connect(server, &QTcpServer::newConnection, [server] {
        while (server->hasPendingConnections()) {
            QTcpSocket *sock = server->nextPendingConnection();
            new SocketHandler(sock);
        }
    });
    if (!server->listen(QHostAddress::Any, quint16(kProxyPort))) {
        return false;
    }
    if (setSystemProxy_) setSystemProxy(true);
    return true;
}

bool stop(bool restoreSystemProxy_)
{
    if (!running()) return true;
    server->close();
    if (restoreSystemProxy_) setSystemProxy(false);
    return true;
}

bool running()
{
    return server && server->isListening();
}

bool doSetup(bool (*askCallback)(), bool *setupDone)
{
    *setupDone = false;
    if (!userConfig) {
        userConfig = new Config(QStringLiteral("Config"), Paths::configDir + "/config.json");
        userConfig->load();
    }
    if (userConfig->readSettingBool("WebProxyInitialSetupRequired")) {
        const bool accepted = askCallback && askCallback();
        userConfig->saveSettingBool("WebProxyInitialSetupRequired", false);
        if (accepted) {
            userConfig->saveSettingBool("WebProxyEnabled", true);
            const bool started = start(true);
            *setupDone = true;
            return started;
        }
        return false;
    }
    if (!userConfig->readSettingBool("WebProxyEnabled")) {
        const bool started = start(false);
        *setupDone = true;
        return started;
    }
    return false;
}

} // namespace WebProxy
