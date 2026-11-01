#pragma once
#include <QString>
#include <QDomDocument>
#include "clientinfo.h"
#include "config.h"

// Port of Client.ChangeGameSettings / Client.ApplyClientSettings: edits the
// per-client GlobalSettings XML templates (config/clients) with the configured
// graphics options and installs them where the client reads them. On Linux
// that is inside the Wine prefix's Roblox AppData directory.
namespace ClientSettings {

void changeGameSettings(const QString &clientName, const Config *userConfig,
                        const ClientInfo &info);

// Port of Client.ApplyClientSettings_custom: computes the graphics mode from
// the user configuration + client load options, then writes every custom
// graphics option into the client's settings XML. Used by the custom
// graphics options dialog.
void applyCustomClientSettings(const ClientInfo &info, const QString &clientName,
                               const Config *userConfig, int meshDetail,
                               int shadingQuality, int materialQuality, int aa,
                               int aaSamples, int bevels, int shadows2008,
                               bool shadows2007, const QString &style2007,
                               int gfxQualityLevel, const QString &windowRes,
                               const QString &fullscreenRes, int modernRes);

// Loads the selected client's settings template (config/clients/*_<client>.xml,
// skipping _default). Returns false when no parsable file exists.
bool loadClientSettingsXml(const QString &clientName, QDomDocument &doc,
                           QString *pathOut = nullptr);

// Port of RobloxXML.GetRenderSettings (simple types): returns the element
// value, or an empty string when the setting does not exist.
QString getRenderSettings(const QDomDocument &doc, const QString &setting,
                          const char *typeName);

} // namespace ClientSettings
