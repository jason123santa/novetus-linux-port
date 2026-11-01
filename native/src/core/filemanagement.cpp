#include "filemanagement.h"
#include "config.h"
#include "paths.h"

#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace {

// NovetusFuncs.GenerateRandomNumber parity (crypto random, mode 0-8).
int generateRandomNumber()
{
    const int mode = int(QRandomGenerator::system()->bounded(0, 8));
    int bound = 0;
    switch (mode) {
    case 0: bound = 9; break;
    case 1: bound = 99; break;
    case 2: bound = 999; break;
    case 3: bound = 9999; break;
    case 4: bound = 99999; break;
    case 5: bound = 999999; break;
    case 6: bound = 9999999; break;
    case 7: bound = 99999999; break;
    }
    if (bound > 0) return int(QRandomGenerator::system()->bounded(bound));
    return int(QRandomGenerator::system()->generate() & 0x7fffffff);
}

} // namespace

namespace FileManagement {

bool resetConfigValues(Config *userConfig, int launcherStyle,
                       const QString &defaultClient, const QString &defaultMap)
{
    // ResetConfigValues: keep the web-proxy opt-in state across the reset.
    const QString proxyInitial = userConfig->readSettingBool("WebProxyInitialSetupRequired")
                                     ? QStringLiteral("True") : QStringLiteral("False");
    const QString proxyEnabled = userConfig->readSettingBool("WebProxyEnabled")
                                     ? QStringLiteral("True") : QStringLiteral("False");

    QFile(Paths::configDir + "/config.json").remove();
    userConfig->load();

    userConfig->saveSetting("SelectedClient", defaultClient);
    userConfig->saveSetting("Map", defaultMap);
    userConfig->saveSetting("CloseOnLaunch", "False");
    userConfig->saveSettingInt("UserID", generateRandomNumber());
    // default name: term from term-list.txt + random number (Config parity)
    QString playerName = "Player";
    QFile termList(Paths::configDir + "/term-list.txt");
    if (termList.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList terms = QString::fromUtf8(termList.readAll())
                                      .split(QRegularExpression("\r\n|\n"), Qt::SkipEmptyParts);
        if (!terms.isEmpty()) {
            playerName = terms[int(QRandomGenerator::system()->bounded(terms.size()))].trimmed()
                         + QString::number(generateRandomNumber());
        }
    }
    userConfig->saveSetting("PlayerName", playerName);
    userConfig->saveSettingInt("RobloxPort", 53640);
    userConfig->saveSettingInt("PlayerLimit", 12);
    userConfig->saveSettingBool("UPnP", false);
    userConfig->saveSettingBool("DisabledAssetSDKHelp", false);
    userConfig->saveSettingBool("DiscordRichPresence", true);
    userConfig->saveSetting("MapPath", Paths::mapsDir + "/" + defaultMap);
    userConfig->saveSetting("MapPathSnip", Paths::mapsDir + "/" + defaultMap);
    userConfig->saveSettingInt("GraphicsMode", 0);
    userConfig->saveSettingInt("QualityLevel", 0);
    userConfig->saveSettingInt("LauncherStyle", launcherStyle);
    userConfig->saveSettingBool("AssetSDKFixerSaveBackups", true);
    userConfig->saveSetting("AlternateServerIP", QString());
    userConfig->saveSetting("ServerBrowserPath", QString());
    userConfig->saveSettingBool("ShowServerNotifications", true);
    userConfig->saveSetting("ServerBrowserServerName", "Novetus");
    userConfig->saveSetting("ServerBrowserServerAddress", QString());
    userConfig->saveSettingInt("Priority", 256);
    userConfig->saveSettingBool("FirstServerLaunch", true);
    userConfig->saveSettingBool("NewGUI", false);
    userConfig->saveSettingBool("URIQuickConfigure", true);
    userConfig->saveSettingBool("BootstrapperShowUI", true);
    userConfig->saveSetting("WebProxyInitialSetupRequired", proxyInitial);
    userConfig->saveSetting("WebProxyEnabled", proxyEnabled);
    userConfig->saveSetting("ClientLaunchTimeOffset", "0.0");
    return true;
}

bool resetCustomizationValues(Config *customConfig)
{
    QFile(Paths::configDir + "/config_customization.json").remove();
    customConfig->load();

    customConfig->saveSetting("Hat1", "NoHat.rbxm");
    customConfig->saveSetting("Hat2", "NoHat.rbxm");
    customConfig->saveSetting("Hat3", "NoHat.rbxm");
    customConfig->saveSetting("Face", "DefaultFace.rbxm");
    customConfig->saveSetting("Head", "DefaultHead.rbxm");
    customConfig->saveSetting("TShirt", "NoTShirt.rbxm");
    customConfig->saveSetting("Shirt", "NoShirt.rbxm");
    customConfig->saveSetting("Pants", "NoPants.rbxm");
    customConfig->saveSetting("Icon", "NBC");
    customConfig->saveSetting("Extra", "NoExtra.rbxm");
    customConfig->saveSettingInt("HeadColorID", 24);
    customConfig->saveSettingInt("TorsoColorID", 23);
    customConfig->saveSettingInt("LeftArmColorID", 24);
    customConfig->saveSettingInt("RightArmColorID", 24);
    customConfig->saveSettingInt("LeftLegColorID", 119);
    customConfig->saveSettingInt("RightLegColorID", 119);
    customConfig->saveSetting("HeadColorString", "Color [A=255, R=245, G=205, B=47]");
    customConfig->saveSetting("TorsoColorString", "Color [A=255, R=13, G=105, B=172]");
    customConfig->saveSetting("LeftArmColorString", "Color [A=255, R=245, G=205, B=47]");
    customConfig->saveSetting("RightArmColorString", "Color [A=255, R=245, G=205, B=47]");
    customConfig->saveSetting("LeftLegColorString", "Color [A=255, R=164, G=189, B=71]");
    customConfig->saveSetting("RightLegColorString", "Color [A=255, R=164, G=189, B=71]");
    customConfig->saveSetting("ExtraSelectionIsHat", "False");
    customConfig->saveSetting("ShowHatsInExtra", "False");
    customConfig->saveSetting("CharacterID", QString());
    return true;
}

} // namespace FileManagement
