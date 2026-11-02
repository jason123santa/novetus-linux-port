#pragma once
#include <QString>
#include <QProcessEnvironment>

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

// Proton/Lutris runner support: when the wine command lives in a runner
// layout (bin/../lib/wine), builtin DLLs outside the standard path
// (vkd3d-proton, dxvk) need WINEDLLPATH set — Proton's own launcher does
// this; running the binary directly does not. Merges the needed entries
// into `env`.
void applyWineEnvironment(QProcessEnvironment &env);
QStringList wineCommandPrefixArgs();

// Convert a POSIX absolute path to the Windows form the Wine client expects
// (drive letter from the prefix's dosdevices; the standard Z: maps to /).
// Non-absolute paths and Windows-style input pass through unchanged.
QString toWinePath(const QString &posixPath);

} // namespace LinuxCompat
