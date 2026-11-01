#include "launcher.h"
#include "linuxcompat.h"
#include "clientsettings.h"
#include "upnp.h"
#include "paths.h"
#include "crypto.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>

Launcher::Launcher(ScriptContext ctx)
    : m_ctx(std::move(ctx))
{
}

void Launcher::log(const QString &msg) const
{
    qInfo().noquote() << msg;
}

QStringList Launcher::listClients() const
{
    QDir dir(Paths::clientDir);
    QStringList out;
    const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &e : entries) {
        if (QFileInfo::exists(Paths::clientDir + "/" + e + "/clientinfo.nov")) {
            out << e;
        }
    }
    return out;
}

bool Launcher::checkMD5(const QString &hash, const QString &path) const
{
    if (!QFileInfo::exists(path)) {
        return false;
    }
    return Crypto::md5File(path).compare(hash, Qt::CaseInsensitive) == 0;
}

bool Launcher::validateFiles(const QString &line, const QString &validstart,
                             const QString &validend)
{
    const QString args = ScriptGen::argsFromTag(line, validstart, validend);
    if (args.trimmed().isEmpty()) {
        return false;
    }
    const QStringList parts = args.split('|');
    if (parts.size() < 2) {
        return false;
    }
    const QString file = parts[0].trimmed();
    const QString hash = parts[1].trimmed();
    const QString path = Paths::clientDir + "/" + m_ctx.clientName + "/" + file;
    if (checkMD5(hash, path)) {
        m_validatedExtraFiles++;
        return true;
    }
    log("ERROR - Failed to launch Novetus. (The client has been detected as modified. [Client.ValidateFiles])");
    return false;
}

bool Launcher::checkClientMD5(const ClientInfo &info, const QString &clientName) const
{
    if (m_ctx.adminMode || info.alreadyHasSecurity) {
        return true;
    }
    const QString dir = Paths::clientDir + "/" + clientName;
    QString exe;
    if (info.legacyMode) {
        exe = dir + "/RobloxApp.exe";
    } else if (info.seperateFolders) {
        exe = dir + "/client/RobloxApp_client.exe";
    } else if (!info.usesCustomClientEXEName) {
        exe = dir + "/RobloxApp_client.exe";
    } else {
        exe = dir + "/" + info.customClientEXEName;
    }
    return checkMD5(info.clientMD5, exe);
}

bool Launcher::checkScriptMD5(const ClientInfo &info, const QString &clientName) const
{
    if (m_ctx.adminMode || info.alreadyHasSecurity) {
        return true;
    }
    return checkMD5(info.scriptMD5,
                    Paths::clientDir + "/" + clientName + "/clientinfo.nov");
}

bool Launcher::decompressMapIfNeeded(ScriptType type, bool nomap)
{
    const QString map = m_ctx.userConfig->readSetting("Map");
    const QString mapPath = m_ctx.userConfig->readSetting("MapPath");
    if (!map.contains(".bz2") || !mapPath.contains(".bz2")) {
        return true;
    }
    const QString decompressed = mapPath.left(mapPath.size() - 4); // ".bz2"
    if (QFileInfo::exists(decompressed)) {
        return true;
    }
    bool doDecompress = nomap;
    if (type == ScriptType_Client || type == ScriptType_Solo || type == ScriptType_OutfitView) {
        doDecompress = true;
    }
    if (!doDecompress && QFileInfo::exists(mapPath)) {
        QProcess bz;
        bz.start("/bin/sh", {"-c", "bunzip2 -c \"$1\" > \"$2\"", "sh", mapPath, decompressed});
        bz.waitForFinished(120000);
        if (bz.exitStatus() != QProcess::NormalExit || bz.exitCode() != 0) {
            log("ERROR - map decompression failed: " + bz.readAllStandardError());
            return false;
        }
        log("Decompressed map to " + decompressed);
        return true;
    }
    return true;
}

QString Launcher::clientExePath(const ClientInfo &info, const QString &clientName,
                                ScriptType type) const
{
    const QString dir = Paths::clientDir + "/" + clientName;
    if (info.legacyMode) {
        return dir + "/RobloxApp.exe";
    }
    if (info.usesCustomClientEXEName) {
        return dir + "/" + info.customClientEXEName;
    }
    if (info.seperateFolders) {
        const QString sub = [&] {
            switch (type) {
            case ScriptType_Client:
            case ScriptType_Solo:
            case ScriptType_OutfitView: return "client";
            case ScriptType_Server:
            case ScriptType_SoloServer: return "server";
            case ScriptType_Studio: return "studio";
            default: return "";
            }
        }();
        switch (type) {
        case ScriptType_Client:
        case ScriptType_Solo: return dir + "/" + sub + "/RobloxApp_client.exe";
        case ScriptType_OutfitView: return dir + "/" + sub + "/RobloxApp_preview.exe";
        case ScriptType_Server:
        case ScriptType_SoloServer: return dir + "/" + sub + "/RobloxApp_server.exe";
        case ScriptType_Studio: return dir + "/" + sub + "/RobloxApp_studio.exe";
        default: return dir + "/RobloxApp.exe";
        }
    }
    switch (type) {
    case ScriptType_Client:
    case ScriptType_Solo: return dir + "/RobloxApp_client.exe";
    case ScriptType_Server:
    case ScriptType_SoloServer: return dir + "/RobloxApp_server.exe";
    case ScriptType_Studio: return dir + "/RobloxApp_studio.exe";
    case ScriptType_OutfitView: return dir + "/RobloxApp_preview.exe";
    default: return dir + "/RobloxApp.exe";
    }
}

void Launcher::resetScripts(const ClientInfo &info, const QString &clientName, ScriptType type) const
{
    const QString launchScript = Paths::clientDir + "/" + clientName
        + (info.seperateFolders ? QString("/%1/content/scripts/").arg(
               type == ScriptType_Server || type == ScriptType_SoloServer ? "server"
               : type == ScriptType_Studio ? "studio" : "client")
                               : "/content/scripts/")
        + Paths::scriptName + ".lua";
    if (QFileInfo::exists(launchScript)) {
        log("Removed Client Launch Script");
        QFile::remove(launchScript);
    }
    if (info.fix2007) {
        // fullReset deletes generated load scripts; LaunchRBXClient uses the
        // default (non-full) reset.
    }
}

QString Launcher::buildLoadout(ScriptType gameOpened) const
{
    // FileManagement.ReloadLoadoutValue (non-localized)
    Config *c = m_ctx.customConfig;
    auto item = [&](const char *key) -> QString {
        const QString v = c->readSetting(key);
        if (gameOpened != ScriptType_Solo && v.endsWith("-Solo.rbxm")) {
            return QByteArray(key) == QByteArray("Extra") ? QString("NoExtra.rbxm")
                                                          : QString("NoHat.rbxm");
        }
        return v;
    };
    const QString hat1 = item("Hat1"), hat2 = item("Hat2"), hat3 = item("Hat3");
    const QString extra = item("Extra");
    const QString colors =
        QString::number(c->readSettingInt("HeadColorID")) + ","
        + QString::number(c->readSettingInt("TorsoColorID")) + ","
        + QString::number(c->readSettingInt("LeftArmColorID")) + ","
        + QString::number(c->readSettingInt("RightArmColorID")) + ","
        + QString::number(c->readSettingInt("LeftLegColorID")) + ","
        + QString::number(c->readSettingInt("RightLegColorID")) + ",'"
        + c->readSetting("TShirt") + "','" + c->readSetting("Shirt") + "','"
        + c->readSetting("Pants") + "','" + c->readSetting("Face") + "','"
        + c->readSetting("Head") + "','" + c->readSetting("Icon") + "','";
    return "'" + hat1 + "','" + hat2 + "','" + hat3 + "'," + colors + extra + "'";
}

bool Launcher::launch(ScriptType type, bool no3d, bool nomap, const QString &mapOverride,
                      QString *errOut, bool internalCall)
{
    auto fail = [&](const QString &msg) {
        if (errOut) *errOut = msg;
        log("ERROR - Failed to launch Novetus. (Error: " + msg + ")");
        return false;
    };

    // C# StartGame(Solo): launch the solo server first, wait for it to load
    // (delay scales with map size, 1.5s minimum), then launch the client with
    // nomap=true so it connects to the local server instead of loading a map.
    if (type == ScriptType_Solo && !internalCall) {
        if (!launch(ScriptType_SoloServer, no3d, false, mapOverride, errOut, true)) {
            return false;
        }
        qint64 mapBytes = 0;
        const QString mapPath = m_ctx.userConfig->readSetting("MapPath");
        if (QFileInfo::exists(mapPath)) {
            mapBytes = QFileInfo(mapPath).size();
        }
        const qint64 megabytes = mapBytes / (1024 * 1024);
        const int delayMs = int(std::max<qint64>(1500, 1500 * (megabytes / 10)));
        log("Waiting " + QString::number(delayMs) + "ms for the solo server to load...");
        QThread::msleep(uint(delayMs));
        return launch(ScriptType_Solo, no3d, true, mapOverride, errOut, true);
    }

    if (!decompressMapIfNeeded(type, nomap)) {
        return fail("map decompression failed");
    }

    m_ctx.loadout = buildLoadout(type);

    ClientInfo info;
    if (!ClientInfoFile::load(Paths::clientDir, m_ctx.clientName, &info)) {
        return fail("could not read clientinfo.nov for " + m_ctx.clientName);
    }

    if (type == ScriptType_Server) {
        log("Pinging master server. Server will now display on the defined master server, if available.");
        // TODO: UDP master-server ping (NovetusFuncs.PingMasterServer)
        if (m_ctx.userConfig->readSettingBool("UPnP")) {
            // NetFuncs.InitUPnP/DeviceFound parity
            const QString altIp = m_ctx.userConfig->readSetting("AlternateServerIP");
            log("UPnP: Service initialized");
            const UPnP::DiscoveryResult result =
                UPnP::openPort(m_ctx.userConfig->readSettingInt("RobloxPort"), altIp);
            if (result.found && result.error.isEmpty()) {
                const QString ip = !result.externalIp.isEmpty() ? result.externalIp : altIp;
                log("UPnP: Port " + QString::number(m_ctx.userConfig->readSettingInt("RobloxPort"))
                    + " opened on '" + ip + "' (UDP/TCP)");
            } else {
                log("UPnP: Unable to register device. Reason - " + result.error);
            }
        }
        if (m_ctx.userConfig->readSettingBool("FirstServerLaunch")) {
            // StartGame parity: first-time hosting tips
            log("Tips for your first time:\n\n"
                "Make sure your server's port forwarded (set up in your router), going through a tunnel server, or running from UPnP.\n"
                "If your port is forwarded or you are going through a tunnel server, make sure your port is set up as UDP, not TCP.\n"
                "Roblox does NOT use TCP, only UDP. However, if your server doesn't work with just UDP, feel free to set up TCP too as that might help the issue in some cases.");
            m_ctx.userConfig->saveSettingBool("FirstServerLaunch", false);
        }
    }

    ClientSettings::changeGameSettings(m_ctx.clientName, m_ctx.userConfig, info);

    resetScripts(info, m_ctx.clientName, type);
    {
        QString err;
        if (!ScriptGen::generateLaunchScriptForClient(m_ctx, type, &err)) {
            return fail(err);
        }
    }

    const QString clientEXEDir = clientExePath(info, m_ctx.clientName, type);
    if (!QFileInfo::exists(clientEXEDir)) {
        return fail("could not find client exe file: " + clientEXEDir);
    }

    // Map resolution (LaunchRBXClient)
    const bool outfitView = (type == ScriptType_OutfitView);
    QString mapFile;    // text4
    QString mapname;    // text3
    if (!nomap) {
        QString mapPath = m_ctx.userConfig->readSetting("MapPath");
        if (!mapOverride.isEmpty()) {
            mapPath = mapOverride;
            // keep the stored settings in sync like the launcher UI does
            m_ctx.userConfig->saveSetting("MapPath", mapPath);
            m_ctx.userConfig->saveSetting("MapPathSnip", "maps/" + QFileInfo(mapPath).fileName());
        }
        mapPath.replace(".bz2", "");
        QString map = m_ctx.userConfig->readSetting("Map");
        map.replace(".bz2", "");
        if (mapOverride.isEmpty()) {
            // MapPath in the config may point under maps/ via MapPathSnip logic;
            // resolve relative names against the maps dir when needed.
            if (!QFileInfo::exists(mapPath) && !map.isEmpty()) {
                const QString candidate = Paths::mapsDir + "/" + map;
                if (QFileInfo::exists(candidate)) {
                    mapPath = candidate;
                }
            }
        }
        mapFile = mapPath;
        mapname = map;
    } else if (type == ScriptType_Studio) {
        mapFile = Paths::dataDir + "/Place1.rbxl";
    }
    if (outfitView) {
        mapFile = Paths::clientDir + "/" + m_ctx.clientName + "/content/fonts/3DView.rbxl";
        mapname.clear();
    }

    // The client is a Windows PE running under Wine: every path embedded in
    // its arguments must be a Windows-style path (Z:\...). File operations on
    // the launcher side keep using the POSIX form.
    mapFile = LinuxCompat::toWinePath(mapFile);

    // Validate extra files listed in CommandLineArgs
    m_validatedExtraFiles = 0;
    m_ctx.validatedExtraFiles = 0;
    const bool v1Args = info.commandLineArgs.contains('<')
                        && info.commandLineArgs.contains("</")
                        && info.commandLineArgs.contains('>');
    if (!m_ctx.adminMode && !info.alreadyHasSecurity) {
        static const QRegularExpression nl("\\r\\n|\\n");
        const QStringList lines = info.commandLineArgs.split(nl, Qt::KeepEmptyParts);
        for (const QString &line : lines) {
            if (line.trimmed().isEmpty()) continue;
            if (v1Args && line.contains("<validate>") && line.contains("</validate>")) {
                if (!validateFiles(line, "<validate>", "</validate>")) {
                    return false;
                }
            } else if (line.contains("validate=")) {
                if (!validateFiles(line, "validate=", "")) {
                    return false;
                }
            }
        }
        m_ctx.validatedExtraFiles = m_validatedExtraFiles;
    }

    // Build the argument string
    const QString quote = "\"";
    QString args;
    if (info.commandLineArgs.contains("%args%")) {
        if (!info.fix2007) {
            args = quote + mapFile + "\" -script \" dofile('rbxasset://scripts/"
                   + Paths::scriptName + ".lua'); "
                   + ScriptGen::scriptFuncForType(m_ctx, type) + quote
                   + (no3d ? " -no3d" : "");
        } else {
            QString err;
            ScriptGen::generateScriptForClient(m_ctx, type, &err);
            // Fix2007 clients get the generated boot script by absolute path;
            // that path must also be in Windows form for the client.
            const QString genScript = LinuxCompat::toWinePath(
                Paths::clientDir + "/" + m_ctx.clientName
                + (info.seperateFolders
                       ? QString("/%1/content/scripts/").arg(
                             type == ScriptType_Server || type == ScriptType_SoloServer
                                 ? "server"
                                 : type == ScriptType_Studio ? "studio" : "client")
                       : "/content/scripts/")
                + Paths::scriptGenName + ".lua");
            args = "-script \"" + genScript + quote
                   + (no3d ? " -no3d" : "") + " " + quote + mapFile + quote;
        }
    } else {
        const bool v1Tags = info.commandLineArgs.contains('<')
                            && info.commandLineArgs.contains("</")
                            && info.commandLineArgs.contains('>');
        args = ScriptGen::compileScript(m_ctx, info,
                                        ScriptGen::tagFromType(type, false, v1Tags),
                                        ScriptGen::tagFromType(type, true, v1Tags),
                                        mapFile,
                                        "rbxasset://scripts/" + Paths::scriptName + ".lua",
                                        clientEXEDir, no3d, type);
    }
    if (args.isEmpty()) {
        return fail("could not compile launch arguments");
    }

    log("Client Loaded.");
    if (type == ScriptType_Client && !m_ctx.adminMode && !info.alreadyHasSecurity) {
        if (!checkClientMD5(info, m_ctx.clientName) || !checkScriptMD5(info, m_ctx.clientName)) {
            log("ERROR - Failed to launch Novetus. (The client has been detected as modified. [Client.LaunchRBXClient])");
            return false;
        }
    }

    return openClient(type, clientEXEDir, args, m_ctx.clientName, mapname);
}

bool Launcher::openClient(ScriptType type, const QString &rbxexe, const QString &args,
                          const QString &clientname, const QString &mapname)
{
    Q_UNUSED(type); Q_UNUSED(clientname); Q_UNUSED(mapname);
    log("Starting Process " + rbxexe);

    // Wine launch (LinuxCompat.StartWindowsProcess)
    const QString winePrefix = LinuxCompat::winePrefix();

    QProcess proc;
    proc.setProgram(LinuxCompat::wineCommandProgram());
    QStringList procArgs;
    procArgs << LinuxCompat::wineCommandPrefixArgs() << rbxexe;
    if (!args.isEmpty()) {
        procArgs << QProcess::splitCommand(args);
    }
    proc.setArguments(procArgs);
    proc.setWorkingDirectory(QFileInfo(rbxexe).absolutePath());
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", winePrefix);
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.startDetached();
    return true;
}
