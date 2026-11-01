#pragma once
#include <QString>
#include <QStringList>
#include "config.h"
#include "clientinfo.h"
#include "script.h"

// Port of the launch path: Client.DecompressMap / ValidateFiles /
// checkClientMD5 / checkScriptMD5 / LaunchRBXClient / OpenClient.
class Launcher
{
public:
    explicit Launcher(ScriptContext ctx);

    QStringList listClients() const;

    // Full launch pipeline. Returns false with a message on failure.
    // internalCall is set for the nested SoloServer phase of a Solo launch.
    bool launch(ScriptType type, bool no3d, bool nomap, const QString &mapOverride,
                QString *errOut, bool internalCall = false);

    void log(const QString &msg) const;

private:
    bool validateFiles(const QString &line, const QString &validstart,
                       const QString &validend);
    bool checkMD5(const QString &hash, const QString &path) const;
    bool checkClientMD5(const ClientInfo &info, const QString &clientName) const;
    bool checkScriptMD5(const ClientInfo &info, const QString &clientName) const;
    bool decompressMapIfNeeded(ScriptType type, bool nomap);
    QString buildLoadout(ScriptType type) const;
    QString clientExePath(const ClientInfo &info, const QString &clientName, ScriptType type) const;
    void resetScripts(const ClientInfo &info, const QString &clientName, ScriptType type) const;
    bool openClient(ScriptType type, const QString &rbxexe, const QString &args,
                    const QString &clientname, const QString &mapname);

    ScriptContext m_ctx;
    int m_validatedExtraFiles = 0;
};
