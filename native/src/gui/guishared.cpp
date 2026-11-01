#include "guishared.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDate>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QMessageBox>
#include <QStringList>
#include <unistd.h>

#include "core/paths.h"
#include "core/launcher.h"

namespace GuiShared {

Ui createConfigs()
{
    Ui ui;
    ui.user = new Config(QStringLiteral("Config"), Paths::configDir + "/config.json");
    ui.custom = new Config(QStringLiteral("Customization"),
                           Paths::configDir + "/config_customization.json");
    ui.user->load();
    ui.custom->load();
    return ui;
}

QString computeTripcode()
{
    char host[256] = {0};
    ::gethostname(host, sizeof(host) - 1);
    QString mac = "00-00-00-00-00-00";
    QDir netDir("/sys/class/net");
    for (const QString &nic : netDir.entryList(QDir::NoDotAndDotDot)) {
        QFile addr("/sys/class/net/" + nic + "/address");
        if (nic == "lo" || !addr.open(QIODevice::ReadOnly)) continue;
        const QString a = QString::fromUtf8(addr.readLine()).trimmed();
        if (!a.isEmpty() && a != "00:00:00:00:00:00") { mac = a; break; }
    }
    return QString::fromLatin1(host) + "|" + QSysInfo::prettyProductName() + "|" + mac;
}

ScriptContext makeContext(const Ui &ui, ScriptType type, const QString &address)
{
    ScriptContext ctx;
    ctx.userConfig = ui.user;
    ctx.customConfig = ui.custom;
    ctx.clientName = ui.user->readSetting("SelectedClient");
    ctx.launcherExePath = QFileInfo("/proc/self/exe").canonicalFilePath();
    ctx.tripcode = computeTripcode();
    QString addr2 = address;
    if (addr2.isEmpty()) {
        addr2 = ui.user->readSetting("IP");
        if (addr2.isEmpty() || addr2 == "N/A") addr2 = "localhost";
    }
    const int colon = addr2.lastIndexOf(':');
    ctx.currentServerIP = colon > 0 ? addr2.left(colon) : addr2;
    ctx.currentServerPort = colon > 0 ? addr2.mid(colon + 1).toInt() : 53640;
    return ctx;
}

bool launchGame(const Ui &ui, ScriptType type, bool no3d, QWidget *msgParent)
{
    QString err;
    Launcher launcher(makeContext(ui, type));
    // C# StartGame parity: join mode passes no map (the server sends it).
    const bool nomap = (type == ScriptType_Client || type == ScriptType_Solo);
    if (!launcher.launch(type, no3d, nomap, QString(), &err)) {
        QMessageBox::critical(msgParent, "Novetus - Error",
                              "Failed to launch Novetus. (Error: "
                              + (err.isEmpty() ? QString("unknown error") : err) + ")");
        return false;
    }
    return true;
}

static QString decodeSplashTokens(QString text, const QString &playerName)
{
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
    QString random;
    const int len = 2 + (int)(QRandomGenerator::global()->generate() % 31);
    for (int i = 0; i < len; i++) {
        random += chars[QRandomGenerator::global()->bounded(chars.size())];
    }
    const QDateTime now = QDateTime::currentDateTime();
    text.replace("%name%", playerName)
        .replace("%randomtext%", random)
        .replace("%version%", productVersion())
        .replace("%year%", QString::number(now.date().year()))
        .replace("%day%", QString::number(now.date().day()))
        .replace("%month%", QString::number(now.date().month()))
        .replace("%nextyear%", QString::number(now.date().year() + 1))
        .replace("%newline%", "\n")
        .replace("%branch%", branchName())
        .replace("[normal]", "")
        .replace("[stylish]", "");
    return text;
}

QString pickSplash(const Config *user)
{
    const QString today = QDate::currentDate().toString("M/d");
    QFile special(Paths::configDir + "/splashes-special.txt");
    if (special.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(special.readAll())
                                      .split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
        for (const QString &l : lines) {
            const QStringList parts = l.split('|');
            if (parts.size() >= 3 && parts[2].trimmed() == today) {
                return parts[0];
            }
        }
    }
    QFile f(Paths::configDir + "/splashes.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(f.readAll())
                                      .split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            return decodeSplashTokens(
                lines[QRandomGenerator::global()->bounded(lines.size())].split('|').first(),
                user ? user->readSetting("PlayerName", true) : QString());
        }
    }
    return "Novetus!";
}

QString productVersion() { return QStringLiteral("26.0.2"); }

const QVector<ContentProviderDef> &contentProviders()
{
    static const QVector<ContentProviderDef> providers = {
        {"Roblox (Asset Delivery, HTTP)", "http://assetdelivery.roblox.com/v1/asset/?id="},
        {"Roblox (Asset Delivery, HTTPS, Incompatible w/ older clients)", "https://assetdelivery.roblox.com/v1/asset/?id="},
        {"Roblox Local (Web Proxy Required)", "http://www.roblox.com/asset?id="},
        {"Imgur (HTTP)", "http://i.imgur.com/"},
        {"Imgur (HTTPS, Incompatible w/ older clients)", "https://i.imgur.com/"},
        {"Novetus Assetdelivery Textures (HTTP)", "http://raw.githubusercontent.com/Novetus/novetus-assetdelivery/master/textures/"},
        {"Novetus Assetdelivery Textures (HTTPS)", "https://raw.githubusercontent.com/Novetus/novetus-assetdelivery/master/textures/"},
        {"Novetus Assetdelivery (HTTP)", "http://raw.githubusercontent.com/Novetus/novetus-assetdelivery/master/"},
        {"Novetus Assetdelivery (HTTPS)", "https://raw.githubusercontent.com/Novetus/novetus-assetdelivery/master/"}
    };
    return providers;
}

QStringList contentProviderNames()
{
    QStringList out;
    for (const ContentProviderDef &p : contentProviders()) {
        out << p.name;
    }
    return out;
}
QString branchName() { return QStringLiteral("1.4"); }

} // namespace GuiShared
