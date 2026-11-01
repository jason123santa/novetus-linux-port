#pragma once
#include <QString>

// Port of Novetus.Core.FileFormat.ClientInfo (legacy clientinfo.nov format:
// one AES-encoded blob containing '|' separated AES-encoded fields).
struct ClientInfo
{
    enum ClientLoadOptionsLegacy {
        Client_2007_NoGraphicsOptions = 0,
        Client_2007,
        Client_2008AndUp,
        Client_2008AndUp_LegacyOpenGL,
        Client_2008AndUp_QualityLevel21,
        Client_2008AndUp_NoGraphicsOptions,
        Client_2008AndUp_ForceAutomatic,
        Client_2008AndUp_ForceAutomaticQL21,
        Client_2008AndUp_HasCharacterOnlyShadowsLegacyOpenGL
    };

    bool usesPlayerName = true;
    bool usesID = true;
    QString description;
    QString warning;
    bool legacyMode = false;
    QString clientMD5;
    QString scriptMD5;
    bool fix2007 = false;
    bool alreadyHasSecurity = false;
    int clientLoadOptions = Client_2008AndUp;
    bool seperateFolders = false;
    bool usesCustomClientEXEName = false;
    QString customClientEXEName;
    QString launchScript;
    double clientLaunchTime = 1.0;
    int clientInfoRevision = 0;
    QString commandLineArgs = "%args%";
};

namespace ClientInfoFile {

// Decodes <clientDir>/<clientName>/clientinfo.nov. Returns false on failure.
bool load(const QString &clientDir, const QString &clientName, ClientInfo *out);

// Exe path resolution mirroring Client.GetClientEXEDir.
QString exeDirFor(const QString &clientDir, const QString &clientName,
                  const ClientInfo &info, int scriptType);

} // namespace ClientInfoFile
