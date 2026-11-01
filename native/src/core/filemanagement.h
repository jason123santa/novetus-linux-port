#pragma once
#include <QString>
class Config;

// Port of Novetus.Core.FileManagement reset helpers.
namespace FileManagement {

// ResetConfigValues parity: rebuilds config.json from defaults, keeps the
// web-proxy opt-in state, resets the map, keeps the launcher style, and
// regenerates the player ID. Returns false when the config could not be
// written.
bool resetConfigValues(Config *userConfig, int launcherStyle,
                       const QString &defaultClient, const QString &defaultMap);

// ResetCustomizationValues parity: rebuilds config_customization.json with
// the default loadout.
bool resetCustomizationValues(Config *customConfig);

} // namespace FileManagement
