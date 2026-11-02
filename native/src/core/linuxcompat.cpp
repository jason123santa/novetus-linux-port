#include "linuxcompat.h"
#include "config.h"
#include "paths.h"

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QFileInfo>

namespace {

// The launcher settings live in config.json; read the two Wine keys lazily
// so changes from the OPTIONS tab apply to the next launch without a restart.
Config *wineConfig()
{
    static Config *cfg = [] {
        Config *c = new Config(QStringLiteral("Config"),
                               Paths::configDir + "/config.json");
        c->load();
        return c;
    }();
    return cfg;
}

} // namespace

namespace LinuxCompat {

bool isLinux()
{
    // This build only targets Linux; kept as an explicit predicate for parity
    // with the C# LinuxCompat.IsLinux checks.
    return true;
}

QString winePrefix()
{
    const QString setting = wineConfig()->readSetting("WinePrefix");
    if (!setting.isEmpty()) {
        return setting;
    }
    const QString env = qEnvironmentVariable("NOVETUS_WINEPREFIX");
    if (!env.isEmpty() && QDir(env).exists()) {
        return env;
    }
    const QString novetusPrefix = QDir::homePath() + "/.novetus/windows_prefix";
    if (QDir(novetusPrefix).exists()) {
        return novetusPrefix;
    }
    return QDir::homePath() + "/.wine";
}

QString wineCommand()
{
    const QString cmd = wineConfig()->readSetting("WineCommand").trimmed();
    return cmd.isEmpty() ? QStringLiteral("wine") : cmd;
}

QStringList wineCommandPrefixArgs()
{
    const QString cmd = wineCommand();
    if (!cmd.contains(QLatin1Char(' '))) return {};
    const QStringList parts = QProcess::splitCommand(cmd);
    return parts.mid(1); // arguments for wrappers like "gamescope -- wine"
}

QString wineCommandProgram()
{
    const QString cmd = wineCommand();
    if (!cmd.contains(QLatin1Char(' '))) return cmd;
    return QProcess::splitCommand(cmd).value(0, QStringLiteral("wine"));
}

QString toWinePath(const QString &posixPath)
{
    if (!posixPath.startsWith(QLatin1Char('/'))) {
        return posixPath; // already Windows-form or relative
    }
    // Standard wine prefixes map Z: -> /. Longest matching dosdevice wins in
    // wine, but / covers everything through Z:.
    QString windows = posixPath;
    windows.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return QLatin1String("Z:") + windows;
}

void applyWineEnvironment(QProcessEnvironment &env)
{
    const QString wine = wineCommandProgram();
    const QDir binDir = QFileInfo(wine).absoluteDir();
    // Runner layout: <runner>/files/bin/wine with libraries in ../lib
    if (binDir.dirName() != "bin" || !binDir.exists("wine")) {
        return;
    }
    QDir libDir(binDir);
    if (!libDir.cdUp() || !QFileInfo(libDir.absoluteFilePath("lib/wine")).isDir()) {
        return;
    }
    const QString lib = libDir.absoluteFilePath("lib");
    QStringList dllPaths;
    for (const char *sub : {"x86_64-windows", "vkd3d-proton", "dxvk",
                            "i386-windows", "x86_64-unix", "i386-unix", "wine"}) {
        const QString p = lib + "/wine/" + sub;
        if (QFileInfo(p).isDir()) dllPaths << p;
    }
    if (dllPaths.isEmpty()) {
        return;
    }
    const QString existing = env.value("WINEDLLPATH");
    QStringList merged = dllPaths;
    if (!existing.isEmpty()) {
        merged << existing;
    }
    env.insert("WINEDLLPATH", merged.join(":"));
}

} // namespace LinuxCompat
