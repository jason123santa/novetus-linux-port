#pragma once
#include <QString>
#include <QStringList>

// Port of Novetus.Core.ModManager (mod package installation/creation) and the
// addon-script registration from its LoadMod.
namespace ModManager {

enum class ModType { ModPackage, AddonScript };

struct InstallResult {
    bool ok = false;
    QString outcome;      // user-facing outcome text (C# installOutcome)
    QStringList fileList; // files the package added/modified
};

// LoadMod parity for ModPackage: extracts a .zip package over the Novetus
// base path (data), silently overwriting, and reports the file list.
InstallResult installModPackage(const QString &zipPath);

// LoadMod parity for AddonScript: copies the script into data/addons and
// registers it in data/addons/core/AddonLoader.lua ("Addons = {...}").
InstallResult installAddonScript(const QString &scriptPath);

// CreateModPackage parity: builds <author> - <name>_info.txt with the
// description and packaged file paths, then zips everything into
// <destDir>/<author> - <name>.zip. Returns the outcome message.
QString createModPackage(const QStringList &filesToPackage, const QString &modName,
                         const QString &modAuthor, const QString &modDesc,
                         const QString &destDir);

} // namespace ModManager
