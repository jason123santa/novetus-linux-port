#include "modmanager.h"
#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace {

QString basePath()
{
    return Paths::basePath;
}

QStringList readZipFileList(const QString &zipPath)
{
    QStringList out;
    QProcess p;
    p.start("unzip", {"-Z1", zipPath});
    p.waitForFinished(15000);
    for (const QString &line : QString::fromUtf8(p.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts)) {
        const QString name = line.trimmed();
        if (!name.isEmpty()) out << name;
    }
    return out;
}

} // namespace

namespace ModManager {

InstallResult installModPackage(const QString &zipPath)
{
    InstallResult r;
    QFileInfo info(zipPath);
    if (!info.exists()) {
        r.outcome = "Error when installing mod: file not found.";
        return r;
    }
    // ZipEntry listing parity: directories are skipped in the report.
    const QStringList entries = readZipFileList(zipPath);
    QStringList files;
    for (const QString &e : entries) {
        if (!e.endsWith('/')) files << e;
    }
    // ExtractAll(BasePath) parity with OverwriteSilently for the loader's
    // "would overwrite" branch.
    QProcess p;
    p.start("unzip", {"-o", "-q", zipPath, "-d", basePath()});
    if (!p.waitForFinished(120000) || p.exitCode() != 0) {
        r.outcome = "Error when installing mod: " + QString::fromUtf8(p.readAllStandardError()).trimmed();
        return r;
    }
    r.ok = true;
    r.fileList = files;
    const int display = 10; // setFileListDisplay(10)
    QString listText = files.join("\n");
    if (files.size() > display) {
        listText = files.mid(0, display).join("\n") + "\n\nand " +
                   QString::number(files.size() - display) + " more files!";
    }
    r.outcome = "Mod " + info.fileName() + " installed! " + QString::number(files.size())
                + " files copied!\nFiles added/modified:\n\n" + listText;
    return r;
}

InstallResult installAddonScript(const QString &scriptPath)
{
    InstallResult r;
    QFileInfo info(scriptPath);
    const QString addonDir = Paths::addonDir;
    const QString loaderPath = Paths::addonCoreDir + "/AddonLoader.lua";
    if (!QFileInfo(loaderPath).exists()) {
        r.outcome = "Error when installing mod: " + loaderPath + " not found.";
        return r;
    }
    const QString dest = addonDir + "/" + info.fileName();
    if (QFileInfo::exists(dest)) {
        r.outcome = "Error: Script has already been added.";
        return r;
    }
    if (!QFile::copy(scriptPath, dest)) {
        r.outcome = "Error when installing mod: cannot copy to " + dest;
        return r;
    }
    // Register in AddonLoader.lua: rewrite the "Addons = {...}" line.
    QFile loader(loaderPath);
    if (!loader.open(QIODevice::ReadOnly | QIODevice::Text)) {
        r.outcome = "Error when installing mod: cannot read " + loaderPath;
        return r;
    }
    QStringList lines = QString::fromUtf8(loader.readAll()).split('\n');
    loader.close();
    const QString baseName = info.completeBaseName();
    bool registered = false;
    for (int i = 0; i < lines.size(); i++) {
        if (lines[i].contains("Addons")) {
            if (lines[i].contains(baseName)) {
                QFile::remove(dest);
                r.outcome = "Error: Script has already been added.";
                return r;
            }
            // strip "Addons = {" ... "}" to a plain CSV list, then append
            QString inner = lines[i];
            inner.remove("Addons").remove("=").remove("{").remove("}").remove(" ");
            QStringList items = inner.split(',', Qt::SkipEmptyParts);
            items << "\"" + baseName + "\"";
            lines[i] = "Addons = {" + items.join(", ") + "}";
            registered = true;
            break;
        }
    }
    if (!registered) {
        QFile::remove(dest);
        r.outcome = "Error when installing mod: no Addons table in " + loaderPath;
        return r;
    }
    if (!loader.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        r.outcome = "Error when installing mod: cannot write " + loaderPath;
        return r;
    }
    loader.write(lines.join("\n").toUtf8());
    loader.close();
    r.ok = true;
    r.outcome = "Addon Script " + info.fileName() + " installed!";
    return r;
}

QString createModPackage(const QStringList &filesToPackage, const QString &modName,
                         const QString &modAuthor, const QString &modDesc,
                         const QString &destDir)
{
    // CreateModPackage parity: staging dir "<author> - <name>", info file,
    // move files in preserving relative paths, zip the staging dir, clean up.
    const QString stageName = modAuthor + " - " + modName;
    const QDir base(basePath());
    const QString stagePath = base.filePath(stageName);
    if (QFileInfo(stagePath).exists()) {
        QDir(stagePath).removeRecursively();
    }
    QDir().mkpath(stagePath);

    {
        QStringList infoLines;
        infoLines << modDesc << "FILE PATHS:";
        for (const QString &f : filesToPackage) {
            infoLines << f;
        }
        QFile infoFile(stagePath + "/" + stageName + "_info.txt");
        if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return "Error when creating mod: cannot write the info file.";
        }
        infoFile.write(infoLines.join("\n").toUtf8());
    }

    int filecount = 0;
    for (const QString &rel : filesToPackage) {
        const QString src = basePath() + rel;
        const QString dst = stagePath + rel;
        QDir().mkpath(QFileInfo(dst).absolutePath());
        if (QFileInfo::exists(dst)) QFile::remove(dst);
        if (!QFile::rename(src, dst)) {
            return "Error when creating mod: cannot move " + src;
        }
        filecount++;
    }

    const QString zipPath = destDir + "/" + stageName + ".zip";
    if (QFileInfo::exists(zipPath)) QFile::remove(zipPath);
    QProcess p;
    p.setWorkingDirectory(stagePath);
    p.start("zip", {"-r", "-q", zipPath, "."});
    if (!p.waitForFinished(120000) || p.exitCode() != 0) {
        return "Error when creating mod: zip failed - "
               + QString::fromUtf8(p.readAllStandardError()).trimmed();
    }
    QDir(stagePath).removeRecursively();
    return QString::number(filecount)
           + " files have been successfully moved and compressed into " + zipPath + "!";
}

} // namespace ModManager
