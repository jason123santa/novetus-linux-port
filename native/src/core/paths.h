#pragma once
#include <QString>

// Port of Novetus.Core.GlobalPaths. The native binary lives in data/bin like
// the original Novetus.exe, so BasePath is the parent of the exe directory.
namespace Paths {

extern QString exeDir;      // .../data/bin
extern QString basePath;    // .../data

extern QString dataPath;        // shareddata
extern QString assetsPath;
extern QString binDir;
extern QString configDir;
extern QString logDir;
extern QString configDirClients;
extern QString configDirTemplates;
extern QString dataDir;
extern QString clientDir;
extern QString mapsDir;
extern QString addonDir;
extern QString addonCoreDir;
extern QString novetusExts;
extern QString mapsDirCustom;
extern QString miscDir;

extern const char *baseGameDir; // "rbxasset://../../../"
extern QString sharedDataGameDir;
extern QString customPlayerDir;

extern QString scriptName;          // CSMPFunctions
extern QString scriptGenName;       // CSMPBoot
extern QString scriptGenSoloName;   // CSMPBootSolo
extern QString scriptGenSoloServerName; // CSMPBootSoloServer
extern QString partColorXmlName;

void init(); // call once at startup

QString gameDirForItem(const QString &sub); // helper

} // namespace Paths
