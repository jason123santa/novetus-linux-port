#include "script.h"
#include "crypto.h"
#include "paths.h"
#include "linuxcompat.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

namespace {

void log(const QString &msg)
{
    qInfo().noquote() << msg;
}

QString lowerBool(const QString &s)
{
    return s.toLower();
}

QString md5OrEmpty(const QString &path)
{
    if (path.isEmpty()) return QString();
    return Crypto::md5File(path);
}

ClientInfo loadInfo(const QString &clientName)
{
    ClientInfo info;
    ClientInfoFile::load(Paths::clientDir, clientName, &info);
    return info;
}

const char *seperateFolderFor(ScriptType type)
{
    // Client.GetClientSeperateFolderName
    switch (type) {
    case ScriptType_Client:
    case ScriptType_Solo:
    case ScriptType_OutfitView: return "client";
    case ScriptType_Server:
    case ScriptType_SoloServer: return "server";
    case ScriptType_Studio: return "studio";
    default: return "";
    }
}

QString genLuaName(const ClientInfo &info, const QString &clientName, ScriptType type)
{
    // Script.GetGenLuaName: rbxasset path used when the client requests it
    const bool useRbxAsset = info.commandLineArgs.contains("%userbxassetforgeneration%");
    QString base = Paths::scriptGenName;
    if (type == ScriptType_Solo) base = Paths::scriptGenSoloName;
    if (type == ScriptType_SoloServer) base = Paths::scriptGenSoloServerName;
    if (!useRbxAsset) {
        if (info.seperateFolders) {
            return Paths::clientDir + "/" + clientName + "/" + seperateFolderFor(type)
                   + "/content/scripts/" + base + ".lua";
        }
        return Paths::clientDir + "/" + clientName + "/content/scripts/" + base + ".lua";
    }
    return "rbxasset://scripts/" + base + ".lua";
}

QString luaFileName(const ClientInfo &info, const QString &clientName, ScriptType type)
{
    // Client.GetLuaFileName
    if (!info.fix2007) {
        return "rbxasset://scripts/" + Paths::scriptName + ".lua";
    }
    return genLuaName(info, clientName, type);
}

QString iconStringToInt(const QString &icon)
{
    if (icon == "BC") return "1";
    if (icon == "TBC") return "2";
    if (icon == "OBC") return "3";
    return "0";
}

} // namespace

namespace ScriptGen {

QString tagFromType(ScriptType type, bool endtag, bool v1)
{
    if (v1) {
        switch (type) {
        case ScriptType_Client:  return endtag ? "</client>" : "<client>";
        case ScriptType_Server:  return endtag ? "</server>" : "<server>";
        case ScriptType_Solo:    return endtag ? "</solo>" : "<solo>";
        case ScriptType_Studio:  return endtag ? "</studio>" : "<studio>";
        default: return QString();
        }
    }
    if (endtag) return QString();
    switch (type) {
    case ScriptType_Client: return "client=";
    case ScriptType_Server: return "server=";
    case ScriptType_Solo: return "solo=";
    case ScriptType_Studio: return "studio=";
    default: return QString();
    }
}

ScriptType typeFromTag(const QString &tag)
{
    if (tag.contains("client")) return ScriptType_Client;
    if (tag.contains("server")) return ScriptType_Server;
    if (tag.contains("solo")) return ScriptType_Solo;
    if (tag.contains("studio")) return ScriptType_Studio;
    return ScriptType_None;
}

QString argsFromTag(const QString &code, const QString &tag, const QString &endtag)
{
    if (endtag.isEmpty()) {
        // SingleOrDefault over lines starting with the tag; 0 or 2+ matches
        // take the C# exception path and return "".
        static const QRegularExpression nl("\\r\\n|\\n");
        const QStringList lines = code.split(nl);
        int matches = 0;
        QString found;
        for (const QString &l : lines) {
            if (l.startsWith(tag)) {
                matches++;
                found = l;
            }
        }
        if (matches != 1) {
            log("Unable to get args from tag " + tag);
            return QString();
        }
        return found.mid(tag.size());
    }
    const int start = code.indexOf(tag) + tag.size();
    const int end = code.lastIndexOf(endtag);
    if (start < (int)tag.size() || end < 0 || end < start) {
        log("Unable to get args from tag " + tag);
        return QString();
    }
    return code.mid(start, end - start);
}

QString nameForType(ScriptType type)
{
    switch (type) {
    case ScriptType_Client: return "Client";
    case ScriptType_Server: return "Server";
    case ScriptType_Solo:
    case ScriptType_SoloServer: return "Play Solo";
    case ScriptType_Studio: return "Studio";
    case ScriptType_OutfitView: return "Avatar 3D Preview";
    default: return "N/A";
    }
}

QString scriptFuncForType(const ScriptContext &ctx, ScriptType type)
{
    const ClientInfo info = loadInfo(ctx.clientName);
    // C# GetScriptFuncForType: the MD5 is always taken over the CLIENT exe,
    // regardless of the script type (server scriptfuncs included).
    const QString exe = info.legacyMode
        ? Paths::clientDir + "/" + ctx.clientName + "/RobloxApp.exe"
        : info.seperateFolders
            ? Paths::clientDir + "/" + ctx.clientName + "/client/RobloxApp_client.exe"
            : info.usesCustomClientEXEName
                ? Paths::clientDir + "/" + ctx.clientName + "/" + info.customClientEXEName
                : Paths::clientDir + "/" + ctx.clientName + "/RobloxApp_client.exe";

    const QString md5Launcher = info.alreadyHasSecurity ? QString() : md5OrEmpty(ctx.launcherExePath);
    const QString md5ClientInfo = info.alreadyHasSecurity
        ? QString()
        : md5OrEmpty(Paths::clientDir + "/" + ctx.clientName + "/clientinfo.nov");
    const QString md5Exe = info.alreadyHasSecurity ? QString() : md5OrEmpty(exe);
    const QString md5s = "'" + md5Exe + "','" + md5Launcher + "','" + md5ClientInfo + "-"
                         + QString::number(info.clientInfoRevision) + "'";

    const bool solo = (type == ScriptType_Solo || type == ScriptType_SoloServer);
    const QString serverIp = solo ? "localhost" : ctx.currentServerIP;
    const int port = solo ? ctx.playSoloPort : ctx.currentServerPort;
    const int robloxPort = solo ? ctx.playSoloPort : ctx.userConfig->readSettingInt("RobloxPort");
    const QString playerLimit = solo ? "1" : ctx.userConfig->readSetting("PlayerLimit");
    const QString notifications = solo
        ? "false"
        : lowerBool(ctx.userConfig->readSetting("ShowServerNotifications"));

    const QString newGui = lowerBool(ctx.userConfig->readSetting("NewGUI"));
    // C# parity: CSConnect closes the tripcode string before the validated-file
    // count (',6,'), CSServer does not (notifications is unquoted there).
    const QString clientExtraFiles = (ctx.validatedExtraFiles > 0)
        ? "'," + QString::number(ctx.validatedExtraFiles) + "," : "',0,";
    const QString serverExtraFiles = (ctx.validatedExtraFiles > 0)
        ? "," + QString::number(ctx.validatedExtraFiles) + "," : ",0,";

    switch (type) {
    case ScriptType_Client:
    case ScriptType_Solo: {
        const QString id = info.usesID
            ? QString::number(ctx.userConfig->readSettingInt("UserID")) : "0";
        const QString name = info.usesPlayerName
            ? ctx.userConfig->readSetting("PlayerName", true) : "Player";
        return "_G.CSConnect(" + id + ",'" + serverIp + "'," + QString::number(port) + ",'"
               + name + "'," + ctx.loadout + "," + md5s + ",'" + ctx.tripcode + clientExtraFiles
               + newGui + ");";
    }
    case ScriptType_Server:
    case ScriptType_SoloServer:
        return "_G.CSServer(" + QString::number(robloxPort) + "," + playerLimit + "," + md5s + ","
               + notifications + serverExtraFiles + newGui + ");";
    case ScriptType_Studio:
        return "_G.CSStudio(" + newGui + ");";
    case ScriptType_OutfitView:
        return "_G.CS3DView(0,'" + ctx.userConfig->readSetting("PlayerName", true) + "',"
               + ctx.loadout + ");";
    default:
        return QString();
    }
}

static QString scriptsDirFor(const ClientInfo &info, const QString &clientName, ScriptType type)
{
    if (info.seperateFolders) {
        return Paths::clientDir + "/" + clientName + "/" + seperateFolderFor(type)
               + "/content/scripts/";
    }
    return Paths::clientDir + "/" + clientName + "/content/scripts/";
}

bool generateLaunchScriptForClient(const ScriptContext &ctx, ScriptType type, QString *err)
{
    const ClientInfo info = loadInfo(ctx.clientName);
    const QString scriptsDir = scriptsDirFor(info, ctx.clientName, type);
    QDir().mkpath(scriptsDir);
    const QString path = scriptsDir + Paths::scriptName + ".lua";
    QFile::remove(path);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = "cannot write " + path;
        return false;
    }
    f.write(QString("--Launch Script\n" + info.launchScript + "\n").toUtf8());
    f.close();
    if (info.commandLineArgs.contains("%signgeneratedjoinscript%")) {
        log("WARNING: script signing (%signgeneratedjoinscript%) is not supported by the native port yet.");
    }
    return true;
}

bool generateScriptForClient(const ScriptContext &ctx, ScriptType type, QString *err)
{
    const ClientInfo info = loadInfo(ctx.clientName);
    const QString scriptsDir = scriptsDirFor(info, ctx.clientName, type);
    QDir().mkpath(scriptsDir);
    QString base = Paths::scriptGenName;
    if (type == ScriptType_Solo) base = Paths::scriptGenSoloName;
    if (type == ScriptType_SoloServer) base = Paths::scriptGenSoloServerName;
    const QString path = scriptsDir + base + ".lua";
    QFile::remove(path);
    const bool useLoadfile = info.commandLineArgs.contains("%useloadfile%");
    const QString loadFn = useLoadfile ? "loadfile" : "dofile";
    const QString doLine = info.seperateFolders
        ? loadFn + "('rbxasset://../../content/scripts/" + Paths::scriptName + ".lua')" + (useLoadfile ? "()" : "")
        : loadFn + "('rbxasset://scripts/" + Paths::scriptName + ".lua')" + (useLoadfile ? "()" : "");
    const QString content = "--Load Script\n" + doLine + "\n"
                            + scriptFuncForType(ctx, type) + "\n";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = "cannot write " + path;
        return false;
    }
    f.write(content.toUtf8());
    f.close();
    if (info.commandLineArgs.contains("%signgeneratedjoinscript%")) {
        log("WARNING: script signing (%signgeneratedjoinscript%) is not supported by the native port yet.");
    }
    return true;
}

QString compileScript(const ScriptContext &ctx, const ClientInfo &info,
                      const QString &tagIn, const QString &endtagIn,
                      const QString &mapfile, const QString &luafile,
                      const QString &rbxexe, bool no3d, ScriptType typeBackup)
{
    QString tag = tagIn;
    QString endtag = endtagIn;
    const QString &code = info.commandLineArgs;

    const bool v1 = code.contains('<') && code.contains("</") && code.contains('>');
    if (!v1 && !endtag.isEmpty()) {
        endtag.clear();
    }

    ScriptType scriptType = typeFromTag(tag);
    bool shared = false;
    if (v1) {
        if (code.contains("<shared>") && code.contains("</shared>")) {
            shared = true;
            tag = "<shared>";
            endtag = "</shared>";
        }
    } else if (code.contains("shared=")) {
        shared = true;
        tag = "shared=";
    }
    if (shared) {
        log("Found shared tags! Launching in shared argument mode.");
    }

    if (scriptType == ScriptType_None) {
        switch (typeBackup) {
        case ScriptType_SoloServer:
            if (!shared) {
                tag = tagFromType(ScriptType_Server, false, v1);
                endtag = tagFromType(ScriptType_Server, true, v1);
            } else {
                scriptType = typeBackup;
            }
            break;
        case ScriptType_OutfitView:
            scriptType = typeBackup;
            break;
        default:
            log("COMPILE ERROR: Client type cannot be loaded.");
            return QString();
        }
    }

    if (v1 && endtag.isEmpty()) {
        log("COMPILE ERROR: Missing end tag for ClientScript v1 client. Client cannot be loaded.");
        return QString();
    }

    if (info.fix2007) {
        QString err;
        generateScriptForClient(ctx, scriptType, &err);
    }

    const QString args = argsFromTag(code, tag, endtag);
    if (args.isEmpty()) {
        log("COMPILE ERROR: No arguments extracted from tag.");
        return QString();
    }
    if (args.contains("%disable%")) {
        log("This option is disabled for this client.");
        return QString();
    }

    const QString md5Launcher = info.alreadyHasSecurity ? QString() : md5OrEmpty(ctx.launcherExePath);
    const QString md5ScriptD = info.alreadyHasSecurity
        ? QString()
        : md5OrEmpty(Paths::clientDir + "/" + ctx.clientName + "/clientinfo.nov");
    const QString md5ExeD = info.alreadyHasSecurity ? QString() : md5OrEmpty(rbxexe);
    const QString newMd5S = "'" + md5ExeD + "','" + md5Launcher + "','" + md5ScriptD + "'";
    const QString md5S = "'" + info.clientMD5 + "','" + md5Launcher + "','" + info.scriptMD5 + "'";

    Config *u = ctx.userConfig;
    Config *c = ctx.customConfig;
    const QString mapPathSnip = QString(u->readSetting("MapPathSnip")).replace(".bz2", "");
    const QString argString = info.fix2007 ? luafile : scriptFuncForType(ctx, scriptType);

    QString out = args;
    struct R { const char *tok; QString val; };
    const QList<R> reps = {
        {"%version%", "26.0.2"},
        {"%mapfile%", mapfile},
        {"%luafile%", luafile},
        {"%charapp%", c->readSetting("CharacterID")},
        {"%server%", ctx.currentServerIP},
        {"%ip%", ctx.currentServerIP},
        {"%port%", u->readSetting("RobloxPort")},
        {"%joinport%", QString::number(ctx.currentServerPort)},
        {"%name%", u->readSetting("PlayerName", true)},
        {"%icone%", iconStringToInt(c->readSetting("Icon"))},
        {"%icon%", c->readSetting("Icon")},
        {"%id%", u->readSetting("UserID")},
        {"%face%", c->readSetting("Face")},
        {"%head%", c->readSetting("Head")},
        {"%tshirt%", c->readSetting("TShirt")},
        {"%shirt%", c->readSetting("Shirt")},
        {"%pants%", c->readSetting("Pants")},
        {"%hat1%", c->readSetting("Hat1")},
        {"%hat2%", c->readSetting("Hat2")},
        {"%hat3%", c->readSetting("Hat3")},
        {"%faced%", c->readSetting("Face").contains("http://")
            ? c->readSetting("Face") : Paths::gameDirForItem("faces") + c->readSetting("Face")},
        {"%headd%", Paths::gameDirForItem("heads") + c->readSetting("Head")},
        {"%tshirtd%", c->readSetting("TShirt").contains("http://")
            ? c->readSetting("TShirt") : Paths::gameDirForItem("tshirts") + c->readSetting("TShirt")},
        {"%shirtd%", c->readSetting("Shirt").contains("http://")
            ? c->readSetting("Shirt") : Paths::gameDirForItem("shirts") + c->readSetting("Shirt")},
        {"%pantsd%", c->readSetting("Pants").contains("http://")
            ? c->readSetting("Pants") : Paths::gameDirForItem("pants") + c->readSetting("Pants")},
        {"%hat1d%", Paths::gameDirForItem("hats") + c->readSetting("Hat1")},
        {"%hat2d%", Paths::gameDirForItem("hats") + c->readSetting("Hat1")},
        {"%hat3d%", Paths::gameDirForItem("hats") + c->readSetting("Hat1")},
        {"%headcolor%", c->readSetting("HeadColorID")},
        {"%torsocolor%", c->readSetting("TorsoColorID")},
        {"%larmcolor%", c->readSetting("LeftArmColorID")},
        {"%llegcolor%", c->readSetting("LeftLegColorID")},
        {"%rarmcolor%", c->readSetting("RightArmColorID")},
        {"%rlegcolor%", c->readSetting("RightLegColorID")},
        {"%md5launcher%", md5Launcher},
        {"%md5script%", info.scriptMD5},
        {"%md5clientinfo%", info.scriptMD5},
        {"%md5exe%", info.clientMD5},
        {"%md5scriptd%", md5ScriptD},
        {"%md5exed%", md5ExeD},
        {"%md5s%", md5S},
        {"%md5sd%", newMd5S},
        {"%limit%", u->readSetting("PlayerLimit")},
        {"%extra%", c->readSetting("Extra")},
        {"%hat4%", c->readSetting("Extra")},
        {"%extrad%", Paths::gameDirForItem("custom") + c->readSetting("Extra")},
        {"%hat4d%", Paths::gameDirForItem("hats") + c->readSetting("Extra")},
        {"%mapfiled%", QString(Paths::baseGameDir) + mapPathSnip},
        {"%tripcode%", ctx.tripcode},
        {"%scripttype%", nameForType(scriptType)},
        {"%notifications%", lowerBool(u->readSetting("ShowServerNotifications"))},
        {"%loadout%", ctx.loadout},
        {"%validatedextrafiles%", QString::number(ctx.validatedExtraFiles)},
        {"%argstring%", argString},
        {"%tshirttexid%", QString()},
        {"%shirttexid%", QString()},
        {"%pantstexid%", QString()},
        {"%facetexid%", QString()},
        {"%tshirttexidlocal%", QString()},
        {"%shirttexidlocal%", QString()},
        {"%pantstexidlocal%", QString()},
        {"%facetexlocal%", QString()},
        {"%newgui%", lowerBool(u->readSetting("NewGUI"))},
        {"%launchscript%", LinuxCompat::toWinePath(
             luaFileName(info, ctx.clientName, scriptType))},
        {"%launchscriptraw%", "rbxasset://scripts/" + Paths::scriptName + ".lua"},
        {"%scriptfunc%", scriptFuncForType(ctx, scriptType)},
        {"%no3d%", no3d ? " -no3d" : QString()},
    };
    for (const R &r : reps) {
        out.replace(QString::fromLatin1(r.tok), r.val);
    }
    return out;
}

} // namespace ScriptGen
