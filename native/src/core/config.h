#pragma once
#include <QString>

// Port of Novetus.Core JSONFile / ConfigBase (FileFormat.Config and
// FileFormat.CustomizationConfig). Values are stored as strings under a
// section object; JsonReadValue searches the whole document by key name.
class Config
{
public:
    explicit Config(const QString &section, const QString &fullPath);

    // Loads (or creates) the file; call before any read.
    bool load();

    QString readSetting(const QString &name, bool removeInvalidChars = false) const;
    int readSettingInt(const QString &name) const;
    bool readSettingBool(const QString &name) const;
    double readSettingDouble(const QString &name) const;

    void saveSetting(const QString &name, const QString &value, bool removeInvalidChars = false);
    void saveSettingInt(const QString &name, int value);
    void saveSettingBool(const QString &name, bool value);

    bool contains(const QString &name) const;

    // Fallbacks when the key is missing/empty (subset of ConfigBase defaults).
    void setDefault(const QString &name, const QString &value);

private:
    QString m_section;
    QString m_path;
    class Impl;
    Impl *d;

    friend class Impl;
};
