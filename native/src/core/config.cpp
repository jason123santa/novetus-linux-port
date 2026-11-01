#include "config.h"
#include "crypto.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDateTime>

class Config::Impl
{
public:
    QJsonObject root;
    // Document-order key walk: mirrors Newtonsoft's obj.Descendants() lookup,
    // where the first property with a matching name (in any section) wins.
    QString findValue(const QJsonObject &obj, const QString &key) const
    {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().isObject()) {
                QString v = findValue(it.value().toObject(), key);
                if (!v.isNull()) return v;
            } else if (it.key() == key) {
                return it.value().toString(QString());
            }
        }
        return QString();
    }

    void writeValue(QJsonObject &obj, const QString &key, const QString &value,
                    const QString &section)
    {
        bool done = false;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().isObject()) {
                QJsonObject sub = it.value().toObject();
                done = writeIfFound(sub, key, value);
                if (done) {
                    obj[it.key()] = sub;
                    break;
                }
            }
        }
        if (!done) {
            QJsonObject sectionObj = obj.value(section).toObject();
            sectionObj[key] = value;
            obj[section] = sectionObj;
        }
    }

    bool writeIfFound(QJsonObject &obj, const QString &key, const QString &value)
    {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().isObject()) {
                QJsonObject sub = it.value().toObject();
                if (writeIfFound(sub, key, value)) {
                    obj[it.key()] = sub;
                    return true;
                }
            } else if (it.key() == key) {
                obj[key] = value;
                return true;
            }
        }
        return false;
    }
};

static QString removeSpecialChars(const QString &s)
{
    // Util.RemoveSpecialCharacters: Regex "[^\w-.'_!()& ]" -> ""
    // (the dash is escaped for PCRE2; it is a literal in .NET's class too)
    static QRegularExpression re(QLatin1String("[^\\w\\-.'_!()& ]"));
    return QString(s).replace(re, QString());
}

Config::Config(const QString &section, const QString &fullPath)
    : m_section(section), m_path(fullPath), d(new Impl)
{
}

bool Config::load()
{
    QFile f(m_path);
    if (!f.exists()) {
        // Create with the section, mirroring JSONFile createNewFile.
        QJsonObject root;
        root[m_section] = QJsonObject{};
        QFile nf(m_path);
        if (nf.open(QIODevice::WriteOnly)) {
            nf.write(QJsonDocument(root).toJson());
            nf.close();
        }
        d->root = root;
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    d->root = doc.object();
    return true;
}

QString Config::readSetting(const QString &name, bool removeInvalidChars) const
{
    QString value = d->findValue(d->root, name);
    if (value.isNull()) {
        return QString();
    }
    if (removeInvalidChars) {
        return removeSpecialChars(value);
    }
    return value;
}

int Config::readSettingInt(const QString &name) const
{
    const QString v = readSetting(name);
    if (v.isEmpty()) return 0;
    bool ok = false;
    int n = v.toInt(&ok);
    return ok ? n : 0;
}

bool Config::readSettingBool(const QString &name) const
{
    // .NET bool.ToString() serializes "True"/"False"
    return readSetting(name).compare(QLatin1String("True"), Qt::CaseInsensitive) == 0;
}

double Config::readSettingDouble(const QString &name) const
{
    const QString v = readSetting(name);
    if (v.isEmpty()) return 0.0;
    bool ok = false;
    double x = v.toDouble(&ok);
    return ok ? x : 0.0;
}

void Config::saveSetting(const QString &name, const QString &value, bool removeInvalidChars)
{
    const QString v = removeInvalidChars ? ::removeSpecialChars(value) : value;
    d->writeValue(d->root, name, v, m_section);
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(d->root).toJson());
    }
}

void Config::saveSettingInt(const QString &name, int value)
{
    saveSetting(name, QString::number(value));
}

void Config::saveSettingBool(const QString &name, bool value)
{
    saveSetting(name, value ? QStringLiteral("True") : QStringLiteral("False"));
}

bool Config::contains(const QString &name) const
{
    return !d->findValue(d->root, name).isNull();
}

void Config::setDefault(const QString &name, const QString &value)
{
    if (!contains(name)) {
        saveSetting(name, value);
    }
}
