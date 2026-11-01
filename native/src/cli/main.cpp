#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

#include "core/paths.h"
#include "core/config.h"
#include "core/crypto.h"
#include "core/script.h"
#include "core/launcher.h"
#include "core/discord.h"

#include <iostream>
#include <unistd.h>

static QString computeTripcode()
{
    // Mirrors NovetusFuncs.GenerateTripcode (machine | OS | MAC).
    char host[256] = {0};
    ::gethostname(host, sizeof(host) - 1);
    const QString machine = QString::fromLatin1(host);
    const QString os = QSysInfo::prettyProductName();
    QString mac = "00-00-00-00-00-00";
    QDir netDir("/sys/class/net");
    for (const QString &nic : netDir.entryList(QDir::NoDotAndDotDot)) {
        QFile addr("/sys/class/net/" + nic + "/address");
        if (nic == "lo" || !addr.open(QIODevice::ReadOnly)) continue;
        const QString a = QString::fromUtf8(addr.readLine()).trimmed();
        if (!a.isEmpty() && a != "00:00:00:00:00:00") { mac = a; break; }
    }
    return machine + "|" + os + "|" + mac;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Paths::init();

    QString client = "2011M";
    QString typeStr = "server";
    QString map;
    bool no3d = false;

    const QStringList args = app.arguments().mid(1);
    for (int i = 0; i < args.size(); i++) {
        const QString &a = args[i];
        if (a == "--client" && i + 1 < args.size()) client = args[++i];
        else if (a == "--map" && i + 1 < args.size()) map = args[++i];
        else if (a == "--type" && i + 1 < args.size()) typeStr = args[++i];
        else if (a == "--no3d") no3d = true;
        else if (a == "--list") {
            Launcher l(ScriptContext{});
            for (const QString &c : l.listClients()) std::cout << c.toStdString() << "\n";
            return 0;
        } else if (a == "--help") {
            std::cout << "novetus-native: native Linux Novetus launcher (no Mono)\n"
                         "  --list                      list installed clients\n"
                         "  --client <name>             client to launch (default 2011M)\n"
                         "  --type client|server|solo|solo-server|studio\n"
                         "  --map <path|name>           map file (default from config)\n"
                         "  --no3d\n";
            return 0;
        }
    }

    ScriptType type = ScriptType_Server;
    if (typeStr == "client") type = ScriptType_Client;
    else if (typeStr == "server") type = ScriptType_Server;
    else if (typeStr == "solo") type = ScriptType_Solo;
    else if (typeStr == "solo-server") type = ScriptType_SoloServer;
    else if (typeStr == "studio") type = ScriptType_Studio;
    else {
        std::cerr << "unknown type: " << typeStr.toStdString() << "\n";
        return 2;
    }

    Config userConfig(QStringLiteral("Config"), Paths::configDir + "/config.json");
    Config customConfig(QStringLiteral("Customization"), Paths::configDir + "/config_customization.json");
    if (!userConfig.load()) {
        std::cerr << "cannot load config.json\n";
        return 2;
    }
    customConfig.load();

    if (userConfig.contains("SelectedClient") && !args.contains("--client")) {
        client = userConfig.readSetting("SelectedClient");
    }

    ScriptContext ctx;
    ctx.userConfig = &userConfig;
    ctx.customConfig = &customConfig;
    ctx.clientName = client;
    ctx.launcherExePath = QFileInfo("/proc/self/exe").canonicalFilePath();
    ctx.tripcode = computeTripcode();
    ctx.currentServerIP = userConfig.readSetting("IP", false);
    if (ctx.currentServerIP.isEmpty() || ctx.currentServerIP == "N/A") {
        ctx.currentServerIP = "localhost";
    }

    // Discord rich presence (best-effort)
    if (userConfig.readSettingBool("DiscordRichPresence")) {
        if (DiscordIpc::initialize("505955125727330324")) {
            DiscordIpc::Presence p;
            p.state = "In Launcher";
            p.details = "Selected " + client;
            p.largeImageKey = "novetus_large";
            p.largeImageText = userConfig.readSetting("PlayerName", true) + " | Novetus 26.0.2";
            p.smallImageKey = "inlauncher_small";
            p.smallImageText = "In Launcher";
            p.startTimestamp = QDateTime::currentSecsSinceEpoch();
            DiscordIpc::updatePresence(p);
        }
    }

    std::cout << "Novetus version EDGE Snapshot v26.0.2 loaded. (native C++ build)\n";
    std::cout << "Novetus path: " << Paths::basePath.toStdString() << "\n";

    Launcher launcher(ctx);
    QString err;
    // C# StartGame parity: join mode passes no map (the server sends it);
    // solo handles its own server/client nomap phases.
    const bool nomap = (type == ScriptType_Client || type == ScriptType_Solo);
    if (!launcher.launch(type, no3d, nomap, map, &err)) {
        std::cerr << "launch failed: " << (err.isEmpty() ? "unknown error" : err.toStdString()) << "\n";
        return 1;
    }

    if (DiscordIpc::initialized()) {
        DiscordIpc::Presence p;
        p.state = type == ScriptType_Server ? "In " + client + " Server"
                : type == ScriptType_Solo ? "In " + client + " Solo Game"
                : "In " + client;
        p.details = map.isEmpty() ? QString("Place1") : QFileInfo(map).fileName();
        p.largeImageKey = "novetus_large";
        p.smallImageKey = "ingame_small";
        p.startTimestamp = QDateTime::currentSecsSinceEpoch();
        DiscordIpc::updatePresence(p);
    }
    DiscordIpc::shutdown();
    return 0;
}
