#include "clientinfo.h"
#include "crypto.h"
#include "paths.h"

#include <QFile>
#include <QDir>
#include <QDebug>

static bool toBoolSafe(const QString &s)
{
    // ConvertSafe.ToBooleanSafe
    return s.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 || s == QLatin1String("1");
}

static int toIntSafe(const QString &s)
{
    bool ok = false;
    int v = s.toInt(&ok);
    return ok ? v : 0;
}

bool ClientInfoFile::load(const QString &clientDir, const QString &clientName, ClientInfo *out)
{
    const QString path = clientDir + "/" + clientName + "/clientinfo.nov";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "clientinfo.nov not found:" << path;
        return false;
    }
    const QString encodedData = QString::fromUtf8(f.readLine()).trimmed();
    const QString text = Crypto::decode(encodedData);
    if (text.isEmpty()) {
        qWarning() << "failed to decode clientinfo.nov:" << path;
        return false;
    }
    const QStringList a = text.split('|');
    if (a.size() < 18) {
        qWarning() << "clientinfo.nov has too few fields:" << path << a.size();
        return false;
    }

    out->usesPlayerName = toBoolSafe(Crypto::decode(a[0]));
    out->usesID = toBoolSafe(Crypto::decode(a[1]));
    out->warning = Crypto::decode(a[2]);
    out->legacyMode = toBoolSafe(Crypto::decode(a[3]));
    out->clientMD5 = Crypto::decode(a[4]);
    // ScriptMD5 is recomputed from the clientinfo file itself on load.
    out->scriptMD5 = Crypto::md5File(path);
    out->description = Crypto::decode(a[6]);
    out->fix2007 = toBoolSafe(Crypto::decode(a[8]));
    out->alreadyHasSecurity = toBoolSafe(Crypto::decode(a[9]));
    out->clientLoadOptions = toIntSafe(Crypto::decode(a[10]));
    out->seperateFolders = toBoolSafe(Crypto::decode(a[11]));
    out->usesCustomClientEXEName = toBoolSafe(Crypto::decode(a[12]));
    out->customClientEXEName = Crypto::decode(a[13]);
    out->commandLineArgs = Crypto::decode(a[14]);
    out->launchScript = Crypto::decode(a[15]);
    out->clientInfoRevision = toIntSafe(Crypto::decode(a[17]));
    return true;
}

QString ClientInfoFile::exeDirFor(const QString &clientDir, const QString &clientName,
                                  const ClientInfo &info, int scriptType)
{
    // ScriptType: Client=0, Server=1, Solo=2, Studio=3, SoloServer=4, OutfitView=5
    const QString dir = clientDir + "/" + clientName;
    if (info.legacyMode) {
        return dir;
    }
    if (info.seperateFolders) {
        const char *sub = "client";
        if (scriptType == 1 || scriptType == 4) {
            sub = "server";
        } else if (scriptType == 3) {
            sub = "studio";
        }
        return dir + "/" + sub;
    }
    return dir;
}
