#pragma once
#include <QString>
#include <functional>

// Native Discord IPC client over the local Unix socket
// ($XDG_RUNTIME_DIR/discord-ipc-N), same protocol as the managed version:
// little-endian int32 opcode + int32 length + JSON payload.
namespace DiscordIpc {

struct Presence
{
    QString state;
    QString details;
    QString largeImageKey;
    QString largeImageText;
    QString smallImageKey;
    QString smallImageText;
    qint64 startTimestamp = 0;
};

bool initialize(const QString &applicationId);
bool initialized();
void shutdown();
void updatePresence(const Presence &p);

} // namespace DiscordIpc
