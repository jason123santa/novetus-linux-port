#include "clientsettings.h"
#include "paths.h"
#include "linuxcompat.h"

#include <QDomDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace {

// ClientInfo.ClientLoadOptionsLegacy
enum ClientLoadOptionsLegacy {
    CLO_Client_2007_NoGraphicsOptions = 0,
    CLO_Client_2007,
    CLO_Client_2008AndUp,
    CLO_Client_2008AndUp_LegacyOpenGL,
    CLO_Client_2008AndUp_QualityLevel21,
    CLO_Client_2008AndUp_NoGraphicsOptions,
    CLO_ForceAutomatic,
    CLO_ForceAutomaticQL21,
    CLO_HasCharacterOnlyShadowsLegacyOpenGL
};

QString winePrefixPath()
{
    return LinuxCompat::winePrefix();
}

QString wineUserName()
{
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty()) user = qEnvironmentVariable("LOGNAME");
    if (user.isEmpty()) user = "jason";
    return user;
}

// GetPathForClientLoadOptions: options 2..8 -> Local AppData, else Roaming.
QStringList settingsDestinations(int clientLoadOptions)
{
    const bool useLocal = (clientLoadOptions >= 2 && clientLoadOptions <= 8);
    QStringList out;
    const QString user = wineUserName();
    const QString sub = useLocal
        ? "AppData/Local/Roblox" : "AppData/Roaming/Roblox";
    out << winePrefixPath() + "/drive_c/users/" + user + "/" + sub;
    // XDG equivalents (parity with what the Mono launcher produced)
    out << (useLocal
        ? QDir::homePath() + "/.local/share/Roblox"
        : QDir::homePath() + "/.config/Roblox");
    return out;
}

// RobloxXML.EditRenderSettings: set <token|float|bool|string|int name="...">
// values inside Item class="RenderSettings".
void editRenderSettings(QDomDocument &doc, const QString &setting,
                        const QString &value, const char *typeName)
{
    QDomNodeList items = doc.elementsByTagName("Item");
    for (int i = 0; i < items.size(); i++) {
        QDomElement item = items.at(i).toElement();
        if (item.isNull() || item.attribute("class") != "RenderSettings") continue;
        QDomNodeList props = item.elementsByTagName(typeName);
        for (int j = 0; j < props.size(); j++) {
            QDomElement e = props.at(j).toElement();
            if (!e.isNull() && e.attribute("name") == setting) {
                while (!e.firstChild().isNull()) e.removeChild(e.firstChild());
                e.appendChild(doc.createTextNode(value));
            }
        }
    }
}

bool loadXml(const QString &path, QDomDocument &doc)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray data = f.readAll();
    // strip a UTF-8 BOM if present
    if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3);
    return (bool)doc.setContent(data);
}

bool saveXml(const QString &path, const QDomDocument &doc)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QByteArray out = doc.toByteArray(2);
    if (!out.startsWith("<?xml")) {
        out = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" + out;
    }
    f.write(out);
    return true;
}

void applyClientSettings(const ClientInfo &info, const QString &clientName,
                         int graphicsMode, int meshDetail, int shadingQuality,
                         int materialQuality, int aa, int aaSamples, int bevels,
                         int shadows2008, bool shadows2007, const QString &style2007,
                         int gfxQualityLevel,
                         const QString &windowRes, const QString &fullscreenRes,
                         int modernRes, bool onlyGraphicsMode)
{
    const QString marker = "_" + clientName;
    QDir cfgClients(Paths::configDir + "/clients");
    for (const QFileInfo &fi : cfgClients.entryInfoList(QDir::Files)) {
        if (!fi.fileName().contains(marker) || fi.fileName().contains("_default")) continue;
        QDomDocument doc;
        if (!loadXml(fi.absoluteFilePath(), doc)) {
            qWarning() << "cannot parse settings template:" << fi.absoluteFilePath();
            break;
        }
        if (graphicsMode != 0) {
            editRenderSettings(doc, "graphicsMode", QString::number(graphicsMode), "token");
        }
        if (!onlyGraphicsMode) {
            editRenderSettings(doc, "maxMeshDetail", QString::number(meshDetail), "float");
            editRenderSettings(doc, "maxShadingQuality", QString::number(shadingQuality), "float");
            editRenderSettings(doc, "minMeshDetail", QString::number(meshDetail), "float");
            editRenderSettings(doc, "minShadingQuality", QString::number(shadingQuality), "float");
            const QString mq = QString::number(materialQuality);
            editRenderSettings(doc, "AluminumQuality", mq, "token");
            editRenderSettings(doc, "CompoundMaterialQuality", mq, "token");
            editRenderSettings(doc, "CorrodedMetalQuality", mq, "token");
            editRenderSettings(doc, "DiamondPlateQuality", mq, "token");
            editRenderSettings(doc, "GrassQuality", mq, "token");
            editRenderSettings(doc, "IceQuality", mq, "token");
            editRenderSettings(doc, "PlasticQuality", mq, "token");
            editRenderSettings(doc, "SlateQuality", mq, "token");
            editRenderSettings(doc, "TrussDetail", mq, "token");
            editRenderSettings(doc, "WoodQuality", mq, "token");
            editRenderSettings(doc, "Antialiasing", QString::number(aa), "token");
            editRenderSettings(doc, "AASamples", QString::number(aaSamples), "token");
            editRenderSettings(doc, "Bevels", QString::number(bevels), "token");
            editRenderSettings(doc, "Shadow", QString::number(shadows2008), "token");
            editRenderSettings(doc, "Shadows", shadows2007 ? "true" : "false", "bool");
            editRenderSettings(doc, "shadows", shadows2007 ? "true" : "false", "bool");
            editRenderSettings(doc, "_skinFile",
                               style2007.isEmpty() ? "" : "Styles/" + style2007, "string");
            editRenderSettings(doc, "QualityLevel", QString::number(gfxQualityLevel), "token");
            editRenderSettings(doc, "FullscreenSizePreference", fullscreenRes, "Vector2int16");
            editRenderSettings(doc, "FullscreenSize", fullscreenRes, "Vector2int16");
            editRenderSettings(doc, "WindowSizePreference", windowRes, "Vector2int16");
            editRenderSettings(doc, "WindowSize", windowRes, "Vector2int16");
            editRenderSettings(doc, "Resolution", QString::number(modernRes), "token");
        }
        if (!saveXml(fi.absoluteFilePath(), doc)) {
            qWarning() << "cannot save settings template:" << fi.absoluteFilePath();
            break;
        }
        const QString renamed = QString(fi.fileName()).replace(marker, "");
        for (const QString &dir : settingsDestinations(info.clientLoadOptions)) {
            QDir().mkpath(dir);
            QFile::remove(dir + "/" + renamed);
            if (!QFile::copy(fi.absoluteFilePath(), dir + "/" + renamed)) {
                qWarning() << "cannot install client settings to" << dir;
            }
        }
    }
}

int graphicsModeFor(const ClientInfo &info, const Config *user)
{
    if (info.clientLoadOptions == CLO_ForceAutomaticQL21
        || info.clientLoadOptions == CLO_ForceAutomatic) {
        return 1;
    }
    // NOTE: the original condition (opts != 0 || opts != NoGraphicsOptions) is
    // always true and is reproduced implicitly here.
    switch (user->readSettingInt("GraphicsMode")) {
    case 1: // OpenGLStable
        switch (info.clientLoadOptions) {
        case CLO_Client_2007:
        case CLO_Client_2008AndUp_LegacyOpenGL:
        case CLO_HasCharacterOnlyShadowsLegacyOpenGL:
            return 2;
        case CLO_Client_2008AndUp:
        case CLO_Client_2008AndUp_QualityLevel21:
            return 4;
        default:
            return 1;
        }
    case 2: // OpenGLExperimental
        return 4;
    case 3: // DirectX
        return 3;
    default: // Automatic
        return 1;
    }
}

} // namespace

namespace ClientSettings {

void changeGameSettings(const QString &clientName, const Config *userConfig,
                        const ClientInfo &info)
{
    // Delete stale settings files listed in FileDeleteFilter.txt
    const QStringList dirs = settingsDestinations(info.clientLoadOptions);
    QFile filter(Paths::configDir + "/FileDeleteFilter.txt");
    if (filter.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList entries = QString::fromUtf8(filter.readAll())
                                        .split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
        for (const QString &entry : entries) {
            for (const QString &dir : dirs) {
                QFile::remove(dir + "/" + entry);
            }
        }
    }

    const int qualityLevel = userConfig->readSettingInt("QualityLevel");
    if (qualityLevel != 6) {
        const int graphicsMode = graphicsModeFor(info, userConfig);
        int meshDetail = 100, shadingQuality = 100, gfxQualityLevel = 19;
        if (info.clientLoadOptions == CLO_ForceAutomaticQL21
            || info.clientLoadOptions == CLO_Client_2008AndUp_QualityLevel21) {
            gfxQualityLevel = 21;
        }
        int materialQuality = 3, aaSamples = 8, bevels = 1, shadows = 1, aa = 1;
        bool shadows2007 = true;
        switch (qualityLevel) {
        case 0: // Automatic
            aa = 0; bevels = 0; shadows = 0; gfxQualityLevel = 0;
            materialQuality = 0; shadows2007 = false;
            break;
        case 1: // VeryLow
            aa = 2; meshDetail = 50; shadingQuality = 50; gfxQualityLevel = 1;
            materialQuality = 1; aaSamples = 1; bevels = 2; shadows = 2; shadows2007 = false;
            break;
        case 2: // Low
            aa = 2; meshDetail = 50; shadingQuality = 50; gfxQualityLevel = 5;
            materialQuality = 1; aaSamples = 1; bevels = 2; shadows = 2; shadows2007 = false;
            break;
        case 3: // Medium
            meshDetail = 75; shadingQuality = 75; gfxQualityLevel = 10;
            materialQuality = 2; aaSamples = 4; bevels = 2;
            if (info.clientLoadOptions == CLO_ForceAutomatic
                || info.clientLoadOptions == CLO_ForceAutomaticQL21
                || info.clientLoadOptions == CLO_Client_2008AndUp_QualityLevel21
                || info.clientLoadOptions == CLO_HasCharacterOnlyShadowsLegacyOpenGL) {
                shadows = 3;
            }
            shadows2007 = false;
            break;
        case 4: // High
            meshDetail = 75; shadingQuality = 75; gfxQualityLevel = 15; aaSamples = 4;
            break;
        default: // Ultra / anything else: keep the initial values
            break;
        }
        applyClientSettings(info, clientName, graphicsMode, meshDetail, shadingQuality,
                            materialQuality, aa, aaSamples, bevels, shadows, shadows2007,
                            QString(), gfxQualityLevel, "800x600", "1024x768", 0, false);
        return;
    }

    // Custom quality: only the graphics mode is applied; the custom options
    // dialog is not ported yet.
    const int graphicsMode = graphicsModeFor(info, userConfig);
    applyClientSettings(info, clientName, graphicsMode, 0, 0, 0, 0, 0, 0, 0,
                        false, QString(), 0, "800x600", "1024x768", 0, true);
    const QString marker = "_" + clientName;
    QDir cfgClients(Paths::configDir + "/clients");
    for (const QFileInfo &fi : cfgClients.entryInfoList(QDir::Files)) {
        if (!fi.fileName().contains(marker) || fi.fileName().contains("_default")) continue;
        const QString renamed = QString(fi.fileName()).replace(marker, "");
        for (const QString &dir : settingsDestinations(info.clientLoadOptions)) {
            QDir().mkpath(dir);
            QFile::remove(dir + "/" + renamed);
            QFile::copy(fi.absoluteFilePath(), dir + "/" + renamed);
        }
    }
}

} // namespace ClientSettings

// ---- public API ------------------------------------------------------------

namespace ClientSettings {

void applyCustomClientSettings(const ClientInfo &info, const QString &clientName,
                               const Config *userConfig, int meshDetail,
                               int shadingQuality, int materialQuality, int aa,
                               int aaSamples, int bevels, int shadows2008,
                               bool shadows2007, const QString &style2007,
                               int gfxQualityLevel, const QString &windowRes,
                               const QString &fullscreenRes, int modernRes)
{
    // ApplyClientSettings_custom: the graphics mode derives from the user
    // configuration + client load options, everything else comes from the dialog.
    const int graphicsMode = graphicsModeFor(info, userConfig);
    applyClientSettings(info, clientName, graphicsMode, meshDetail, shadingQuality,
                        materialQuality, aa, aaSamples, bevels, shadows2008,
                        shadows2007, style2007, gfxQualityLevel, windowRes,
                        fullscreenRes, modernRes, false);
}

bool loadClientSettingsXml(const QString &clientName, QDomDocument &doc,
                           QString *pathOut)
{
    const QString marker = "_" + clientName;
    QDir cfgClients(Paths::configDir + "/clients");
    for (const QFileInfo &fi : cfgClients.entryInfoList(QDir::Files)) {
        if (!fi.fileName().contains(marker) || fi.fileName().contains("_default")) {
            continue;
        }
        if (loadXml(fi.absoluteFilePath(), doc)) {
            if (pathOut) *pathOut = fi.absoluteFilePath();
            return true;
        }
        qWarning() << "cannot parse settings template:" << fi.absoluteFilePath();
        return false;
    }
    return false;
}

QString getRenderSettings(const QDomDocument &doc, const QString &setting,
                          const char *typeName)
{
    // RobloxXML.GetRenderSettings parity: the first <typeName name="setting">
    // inside Item class="RenderSettings". Vector2int16 values are returned as
    // "XxY" like the C# version assembles them.
    QDomNodeList items = doc.elementsByTagName("Item");
    for (int i = 0; i < items.size(); i++) {
        QDomElement item = items.at(i).toElement();
        if (item.isNull() || item.attribute("class") != "RenderSettings") continue;
        QDomNodeList props = item.elementsByTagName(typeName);
        for (int j = 0; j < props.size(); j++) {
            QDomElement e = props.at(j).toElement();
            if (!e.isNull() && e.attribute("name") == setting) {
                if (qstrcmp(typeName, "Vector2int16") != 0) {
                    return e.text().trimmed();
                }
                const QStringList children = {"X", "Y"};
                QStringList values;
                for (const QString &child : children) {
                    QDomNodeList x = e.elementsByTagName(child);
                    if (x.isEmpty()) return QString();
                    values << x.at(0).toElement().text().trimmed();
                }
                return values[0] + "x" + values[1];
            }
        }
    }
    return QString();
}

} // namespace ClientSettings
