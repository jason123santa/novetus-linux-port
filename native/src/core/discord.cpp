#include "discord.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QUuid>
#include <QDebug>
#include <QDateTime>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

namespace DiscordIpc {

namespace {

std::mutex g_mutex;
int g_fd = -1;
std::thread g_reader;
std::atomic<bool> g_running{false};
QString g_appId;

bool readFull(int fd, void *buf, size_t n)
{
    auto *p = static_cast<char *>(buf);
    size_t got = 0;
    while (got < n) {
        ssize_t r = ::read(fd, p + got, n - got);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

bool sendFrame(int fd, int op, const QJsonObject &payload)
{
    if (fd < 0) return false;
    const QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    unsigned char header[8];
    header[0] = op & 0xFF; header[1] = (op >> 8) & 0xFF;
    header[2] = (op >> 16) & 0xFF; header[3] = (op >> 24) & 0xFF;
    const int len = data.size();
    header[4] = len & 0xFF; header[5] = (len >> 8) & 0xFF;
    header[6] = (len >> 16) & 0xFF; header[7] = (len >> 24) & 0xFF;
    ssize_t w = ::write(fd, header, 8);
    if (w != 8) return false;
    return ::write(fd, data.constData(), data.size()) == data.size();
}

void readerLoop()
{
    while (g_running.load()) {
        unsigned char header[8];
        if (!readFull(g_fd, header, 8)) break;
        const int op = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
        int len = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);
        if (len < 0 || len > 1048576) break;
        std::string payload(len, '\0');
        if (len > 0 && !readFull(g_fd, payload.data(), len)) break;
        if (op == 2) break; // server asked to close
        // op 1 dispatches (READY etc.); nothing to do beyond keeping alive.
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
}

QString findSocketPath()
{
    QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QString("/run/user/%1").arg(::getuid());
    }
    for (int i = 0; i < 10; i++) {
        const QString candidate = runtimeDir + "/discord-ipc-" + QString::number(i);
        if (QFile::exists(candidate)) return candidate;
        const QString flatpak = runtimeDir + "/app/com.discordapp.Discord/discord-ipc-"
                                + QString::number(i);
        if (QFile::exists(flatpak)) return flatpak;
    }
    return QString();
}

} // namespace

bool initialize(const QString &applicationId)
{
    const QString socketPath = findSocketPath();
    if (socketPath.isEmpty()) {
        qInfo().noquote() << "Discord RPC: IPC socket not found (is Discord running?). Rich presence disabled.";
        return false;
    }
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const QByteArray pathBytes = socketPath.toUtf8();
    if (pathBytes.size() >= (int)sizeof(addr.sun_path)) {
        ::close(fd);
        return false;
    }
    std::memcpy(addr.sun_path, pathBytes.constData(), pathBytes.size() + 1);
    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        qInfo().noquote() << "Discord RPC: connect failed:" << strerror(errno);
        return false;
    }
    QJsonObject handshake;
    handshake["v"] = 1;
    handshake["client_id"] = applicationId;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_fd = fd;
        g_appId = applicationId;
        if (!sendFrame(g_fd, 0, handshake)) {
            ::close(g_fd);
            g_fd = -1;
            return false;
        }
    }
    g_running = true;
    g_reader = std::thread(readerLoop);
    qInfo().noquote() << "Discord RPC: Connected via" << socketPath;
    return true;
}

bool initialized()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_fd >= 0;
}

void shutdown()
{
    g_running = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_fd >= 0) {
            ::close(g_fd); // unblocks the reader
            g_fd = -1;
        }
    }
    if (g_reader.joinable()) {
        g_reader.join();
    }
}

void updatePresence(const Presence &p)
{
    QJsonObject activity;
    if (!p.state.isEmpty()) activity["state"] = p.state;
    if (!p.details.isEmpty()) activity["details"] = p.details;
    if (p.startTimestamp > 0) {
        QJsonObject ts;
        ts["start"] = p.startTimestamp;
        activity["timestamps"] = ts;
    }
    if (!p.largeImageKey.isEmpty() || !p.smallImageKey.isEmpty()) {
        QJsonObject assets;
        if (!p.largeImageKey.isEmpty()) assets["large_image"] = p.largeImageKey;
        if (!p.largeImageText.isEmpty()) assets["large_text"] = p.largeImageText;
        if (!p.smallImageKey.isEmpty()) assets["small_image"] = p.smallImageKey;
        if (!p.smallImageText.isEmpty()) assets["small_text"] = p.smallImageText;
        activity["assets"] = assets;
    }
    QJsonObject args;
    args["pid"] = (int)::getpid();
    args["activity"] = activity;
    QJsonObject payload;
    payload["cmd"] = "SET_ACTIVITY";
    payload["args"] = args;
    payload["nonce"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::lock_guard<std::mutex> lock(g_mutex);
    sendFrame(g_fd, 1, payload);
}

} // namespace DiscordIpc
