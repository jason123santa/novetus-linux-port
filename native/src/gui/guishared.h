#pragma once
#include <QString>
#include <QWidget>
#include "core/config.h"
#include "core/script.h"

// Shared helpers for the Qt launcher windows (the role LauncherFormShared
// plays in the original: config access, launching, splash selection).
namespace GuiShared {

struct Ui
{
    Config *user = nullptr;
    Config *custom = nullptr;
};

// Novetus.Core.ContentProvider parity (web sources for custom items)
struct ContentProviderDef {
    QString name;
    QString url;
};
const QVector<ContentProviderDef> &contentProviders();

Ui createConfigs();

ScriptContext makeContext(const Ui &ui, ScriptType type, const QString &address = QString());
bool launchGame(const Ui &ui, ScriptType type, bool no3d, QWidget *msgParent);
QString computeTripcode();
QString pickSplash(const Config *user); // date-based special splash else random
QString productVersion();      // "26.0.2"
const QVector<ContentProviderDef> &contentProviders();
QStringList contentProviderNames();
QString branchName();          // "1.4"

} // namespace GuiShared
