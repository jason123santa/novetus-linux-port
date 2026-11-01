#pragma once
#include <QString>
#include "config.h"
#include "clientinfo.h"

enum ScriptType
{
    ScriptType_Client = 0,
    ScriptType_Server = 1,
    ScriptType_Solo = 2,
    ScriptType_Studio = 3,
    ScriptType_SoloServer = 4,
    ScriptType_OutfitView = 5,
    ScriptType_None = 6
};

// Everything the script compiler and launcher need, mirroring the GlobalVars
// state the C# code relies on.
struct ScriptContext
{
    Config *userConfig = nullptr;        // config.json  ("Config")
    Config *customConfig = nullptr;      // config_customization.json
    QString clientName;
    QString launcherExePath;             // this binary; used for %md5launcher%
    QString currentServerIP = "localhost";
    int currentServerPort = 53640;
    int playSoloPort = 1027;
    QString loadout;
    QString tripcode;
    int validatedExtraFiles = 0;
    bool adminMode = false;
    bool easterEggMode = false;
};

namespace ScriptGen {

QString tagFromType(ScriptType type, bool endtag, bool v1);
ScriptType typeFromTag(const QString &tag);
QString argsFromTag(const QString &code, const QString &tag, const QString &endtag);

QString nameForType(ScriptType type);

// Script.Generator
QString scriptFuncForType(const ScriptContext &ctx, ScriptType type);
bool generateLaunchScriptForClient(const ScriptContext &ctx, ScriptType type, QString *err);
bool generateScriptForClient(const ScriptContext &ctx, ScriptType type, QString *err);

// Script.ClientScript.CompileScript — returns the final argument string, or
// an empty string on error (message already logged).
QString compileScript(const ScriptContext &ctx, const ClientInfo &info,
                      const QString &tag, const QString &endtag,
                      const QString &mapfile, const QString &luafile,
                      const QString &rbxexe, bool no3d, ScriptType typeBackup);

} // namespace ScriptGen
