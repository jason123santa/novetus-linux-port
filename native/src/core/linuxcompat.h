#pragma once
#include <QString>

// Linux-specific runtime helpers (Wine integration).
namespace LinuxCompat {

bool isLinux();

// Wine prefix: config.json "WinePrefix" -> $NOVETUS_WINEPREFIX ->
// ~/.novetus/windows_prefix -> ~/.wine
QString winePrefix();

// Wine command: config.json "WineCommand" (default "wine"). A value with
// spaces is split into program + argument prefix (wrappers like gamescope).
QString wineCommand();
QString wineCommandProgram();     // first token of wineCommand()
QStringList wineCommandPrefixArgs();

// Convert a POSIX absolute path to the Windows form the Wine client expects
// (drive letter from the prefix's dosdevices; the standard Z: maps to /).
// Non-absolute paths and Windows-style input pass through unchanged.
QString toWinePath(const QString &posixPath);

} // namespace LinuxCompat
