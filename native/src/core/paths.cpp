#include "paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace Paths {

QString exeDir;
QString basePath;

QString dataPath, assetsPath, binDir, configDir, logDir;
QString configDirClients, configDirTemplates, dataDir, clientDir, mapsDir;
QString addonDir, addonCoreDir, novetusExts, mapsDirCustom, miscDir;

const char *baseGameDir = "rbxasset://../../../";
QString sharedDataGameDir;
QString customPlayerDir;

QString scriptName = "CSMPFunctions";
QString scriptGenName = "CSMPBoot";
QString scriptGenSoloName = "CSMPBootSolo";
QString scriptGenSoloServerName = "CSMPBootSoloServer";
QString partColorXmlName = "PartColors.json";

void init()
{
    QString exe = QFileInfo("/proc/self/exe").canonicalFilePath();
    if (exe.isEmpty()) {
        exe = QCoreApplication::applicationFilePath();
    }
    exeDir = QFileInfo(exe).absolutePath();
    // Original layout: <root>/data/bin/<exe>  ->  base = <root>/data
    QDir base(exeDir);
    base.cdUp();
    basePath = base.absolutePath();

    dataPath = basePath + "/shareddata";
    assetsPath = basePath + "/assets";
    binDir = basePath + "/bin";
    configDir = basePath + "/config";
    logDir = basePath + "/logs";
    configDirClients = configDir + "/clients";
    configDirTemplates = configDir + "/itemtemplates";
    dataDir = configDir + "/launcherdata";
    clientDir = basePath + "/clients";
    mapsDir = basePath + "/maps";
    addonDir = basePath + "/addons";
    addonCoreDir = addonDir + "/core";
    novetusExts = basePath + "/extensions";
    mapsDirCustom = mapsDir + "/Custom";
    miscDir = basePath + "/misc";

    sharedDataGameDir = QString(baseGameDir) + "shareddata/";
    customPlayerDir = dataPath + "/charcustom";
}

QString gameDirForItem(const QString &sub)
{
    return sharedDataGameDir + sub + "/";
}

} // namespace Paths
