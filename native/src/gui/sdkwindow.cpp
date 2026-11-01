#include "sdkwindow.h"

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextBrowser>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QRegularExpression>

#include "core/paths.h"
#include "core/config.h"
#include "core/modmanager.h"
#include "core/crypto.h"
#include "core/clientinfo.h"
#include "core/linuxcompat.h"
#include "guishared.h"

namespace {

// ContentProvider.GetContentProviders parity for URL-bearing providers.
struct DownloadProvider {
    QString name;
    QString url;
    bool website;
};

QVector<DownloadProvider> downloadProviders()
{
    QVector<DownloadProvider> out;
    for (const GuiShared::ContentProviderDef &p : GuiShared::contentProviders()) {
        if (p.url.contains("?id=")) {
            out << DownloadProvider{p.name, p.url, false};
        }
    }
    out << DownloadProvider{"https://www.roblox.com/catalog/",
                            "https://www.roblox.com/catalog/", true};
    out << DownloadProvider{"https://www.roblox.com/library/",
                            "https://www.roblox.com/library/", true};
    return out;
}

QString validExtension()
{
    return ".rbxm"; // StartItemDownload's default extension
}

} // namespace

// ---- Client Script Documentation (ClientScriptDocumentation.cs) ------------
DocsWindow::DocsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("ClientScript Documentation");
    resize(660, 360);
    auto *view = new QTextBrowser(this);
    QFile f(Paths::basePath + "/misc/documentation.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        view->setPlainText(QString::fromUtf8(f.readAll()));
    } else {
        view->setPlainText("ERROR - " + Paths::basePath
                           + "/misc/documentation.txt not found.");
    }
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->addWidget(view);
}

// ---- Splash Tester (SplashTester.cs) ---------------------------------------
SplashTesterWindow::SplashTesterWindow(const Config *user, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Splash Tester");
    resize(420, 140);
    auto *lay = new QVBoxLayout(this);
    auto *label = new QLabel(this);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setText("Splash: " + GuiShared::pickSplash(user));
    lay->addWidget(label);
    auto *again = new QPushButton("Another splash", this);
    connect(again, &QPushButton::clicked, this, [this, label, user] {
        label->setText("Splash: " + GuiShared::pickSplash(user));
    });
    lay->addWidget(again);
}

// ---- Asset Downloader (AssetDownloader.cs) ---------------------------------
class AssetDownloaderWindow : public QDialog
{
public:
    explicit AssetDownloaderWindow(Config *user, QWidget *parent)
        : QDialog(parent), m_user(user)
    {
        setWindowTitle("Asset Downloader");
        setFixedWidth(520);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 12);
        lay->setSpacing(10);

        auto *form = new QFormLayout();
        form->setSpacing(8);
        m_url = new QComboBox(this);
        for (const DownloadProvider &p : downloadProviders()) {
            m_url->addItem(p.name);
        }
        m_url->setToolTip("URL override must end with 'asset/?id='");
        m_name = new QLineEdit(this);
        m_id = new QLineEdit(this);
        m_version = new QSpinBox(this);
        m_version->setRange(0, 9999);
        m_override = new QLineEdit(this);
        form->addRow("URL:", m_url);
        form->addRow("Asset name:", m_name);
        form->addRow("Asset ID:", m_id);
        form->addRow("Version (0 = latest):", m_version);
        form->addRow("URL override:", m_override);
        lay->addLayout(form);

        m_batch = new QCheckBox("Batch mode (name|id|version per line)", this);
        lay->addWidget(m_batch);
        m_batchBox = new QPlainTextEdit(this);
        m_batchBox->setVisible(false);
        m_batchBox->setPlaceholderText("hat1|1815534|0\nhat2|13657696|0");
        m_batchBox->setMaximumHeight(120);
        lay->addWidget(m_batchBox);
        connect(m_batch, &QCheckBox::toggled, m_batchBox, &QPlainTextEdit::setVisible);

        m_noHelp = new QCheckBox("Don't show the extension help message", this);
        m_noHelp->setChecked(user->readSettingBool("DisabledAssetSDKHelp"));
        lay->addWidget(m_noHelp);
        connect(m_noHelp, &QCheckBox::toggled, this, [this](bool on) {
            m_user->saveSettingBool("DisabledAssetSDKHelp", on);
        });

        auto *btnRow = new QHBoxLayout();
        auto *go = new QPushButton("Download", this);
        connect(go, &QPushButton::clicked, this, &AssetDownloaderWindow::download);
        btnRow->addStretch(1);
        btnRow->addWidget(go);
        lay->addLayout(btnRow);
        m_status = new QLabel(this);
        lay->addWidget(m_status);
    }

private:
    QString currentUrl(bool *isWebsite) const
    {
        if (!m_override->text().trimmed().isEmpty()) {
            *isWebsite = false;
            return m_override->text().trimmed();
        }
        const DownloadProvider p = downloadProviders()[m_url->currentIndex()];
        *isWebsite = p.website;
        return p.url;
    }

    void download()
    {
        bool isWebsite = false;
        const QString url = currentUrl(&isWebsite);
        if (m_batch->isChecked()) {
            if (isWebsite) {
                QMessageBox::warning(this, "Asset Downloader",
                                     "The Batch Downloader is not made for loading websites.");
                return;
            }
            batchDownload(url);
            return;
        }
        if (isWebsite) {
            QDesktopServices::openUrl(QUrl(url + m_id->text().trimmed()));
            return;
        }
        singleDownload(m_name->text().trimmed(), url, m_id->text().trimmed(),
                       m_version->value());
    }

    void singleDownload(const QString &name, const QString &url, const QString &id, int ver)
    {
        if (!m_user->readSettingBool("DisabledAssetSDKHelp")) {
            QMessageBox::information(this, "Asset Downloader",
                "If you're trying to create a offline item, please use these file extension "
                "names when saving your files:\n.rbxm - Roblox Model/Item\n.rbxl - Roblox Place"
                "\n.mesh - Roblox Mesh\n.png - Texture/Icon\n.wav - Sound\n.lua - Lua Script");
        }
        const QString filter =
            "Roblox Model (*.rbxm);;Roblox Place (*.rbxl);;Roblox Mesh (*.mesh)"
            ";;PNG Image (*.png);;WAV Sound (*.wav);;Lua Script (*.lua)";
        QString path = QFileDialog::getSaveFileName(this, "Save asset", name, filter);
        if (path.isEmpty()) return;
        m_status->setText("Downloading " + id + "...");
        fetch(url + id + (ver != 0 ? "&version=" + QString::number(ver) : ""),
              [this, path](const QByteArray &data, const QString &err) {
                  if (!err.isEmpty()) {
                      m_status->clear();
                      QMessageBox::warning(this, "Asset Downloader - Error",
                                           "Error: Unable to download the file. " + err);
                      return;
                  }
                  QFile f(path);
                  if (!f.open(QIODevice::WriteOnly)) {
                      m_status->clear();
                      QMessageBox::warning(this, "Asset Downloader - Error",
                                           "Error: cannot write " + path);
                      return;
                  }
                  f.write(data);
                  f.close();
                  m_status->setText("Downloaded " + QFileInfo(path).fileName()
                                    + " (" + QString::number(data.size()) + " bytes).");
              });
    }

    void batchDownload(const QString &url)
    {
        const QString filter =
            "Roblox Model (*.rbxm);;Roblox Place (*.rbxl);;Roblox Mesh (*.mesh)"
            ";;PNG Image (*.png);;WAV Sound (*.wav)";
        QString chosen = QFileDialog::getSaveFileName(
            this, "Save files downloaded via batch", "Choose batch download location.", filter);
        if (chosen.isEmpty()) return;
        const QString dir = QFileInfo(chosen).absolutePath();
        const QString ext = QFileInfo(chosen).suffix().isEmpty()
                                ? validExtension()
                                : "." + QFileInfo(chosen).suffix();
        const QStringList lines = m_batchBox->toPlainText().split('\n', Qt::SkipEmptyParts);
        int ok = 0, errors = 0;
        qint64 total = 0;
        QProgressDialog progress("Downloading batch...", QString(), 0, lines.size(), this);
        progress.setWindowModality(Qt::WindowModal);
        for (int i = 0; i < lines.size(); i++) {
            progress.setValue(i);
            const QStringList parts = lines[i].trimmed().split('|');
            if (parts.size() < 2) { errors++; continue; }
            const QString name = parts[0].trimmed();
            const QString id = parts[1].trimmed();
            const int ver = parts.size() > 2 ? parts[2].trimmed().toInt() : 0;
            const QByteArray data = fetchSync(url + id
                                              + (ver != 0 ? "&version=" + QString::number(ver) : ""));
            if (data.isEmpty()) { errors++; continue; }
            QFile f(dir + "/" + name + ext);
            if (!f.open(QIODevice::WriteOnly)) { errors++; continue; }
            f.write(data);
            f.close();
            total += data.size();
            ok++;
        }
        progress.setValue(lines.size());
        QString errText;
        if (errors > 0) {
            errText = "\n" + QString::number(errors)
                      + " errors were detected during the download. Make sure your IDs and links are valid.";
        }
        QMessageBox::information(this, "Asset Downloader - Download Complete",
            "Batch download complete! " + QString::number(ok) + " items downloaded! "
            + QString::number(total) + " bytes written!" + errText);
        m_status->clear();
    }

    void fetch(const QString &url, std::function<void(const QByteArray &, const QString &)> done)
    {
        auto *nam = new QNetworkAccessManager(this);
        QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                done(QByteArray(), reply->errorString());
                return;
            }
            done(reply->readAll(), QString());
        });
    }

    QByteArray fetchSync(const QString &url)
    {
        QNetworkAccessManager nam;
        QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(url)));
        QEventLoop loop;
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        const QByteArray data = reply->error() == QNetworkReply::NoError
                                    ? reply->readAll() : QByteArray();
        reply->deleteLater();
        return data;
    }

    Config *m_user;
    QComboBox *m_url = nullptr;
    QLineEdit *m_name = nullptr, *m_id = nullptr, *m_override = nullptr;
    QSpinBox *m_version = nullptr;
    QCheckBox *m_batch = nullptr, *m_noHelp = nullptr;
    QPlainTextEdit *m_batchBox = nullptr;
    QLabel *m_status = nullptr;
};

// ---- Asset Fixer (AssetFixer.cs) -------------------------------------------
class AssetFixerWindow : public QDialog
{
public:
    explicit AssetFixerWindow(Config *user, QWidget *parent)
        : QDialog(parent), m_user(user)
    {
        setWindowTitle("Asset Fixer");
        setFixedWidth(520);
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 12);
        lay->setSpacing(10);
        auto *info = new QLabel(
            "Localizes the assets referenced by an XML place or model (.rbxlx/.rbxlx.bz2/"
            ".rbxmx). Referenced assets are downloaded into the asset cache and the "
            "URLs are rewritten to local assetgame:// paths.", this);
        info->setWordWrap(true);
        lay->addWidget(info);

        m_path = new QLineEdit(this);
        auto *browse = new QPushButton("Browse...", this);
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString f = QFileDialog::getOpenFileName(
                this, "Open a place or model",
                Paths::mapsDir,
                "XML places/models (*.rbxlx *.rbxlx.bz2 *.rbxmx)");
            if (!f.isEmpty()) m_path->setText(f);
        });
        auto *pathRow = new QHBoxLayout();
        pathRow->addWidget(new QLabel("File:", this));
        pathRow->addWidget(m_path, 1);
        pathRow->addWidget(browse);
        lay->addLayout(pathRow);

        m_backup = new QCheckBox("Save backups (.bak)", this);
        m_backup->setChecked(user->readSettingBool("AssetSDKFixerSaveBackups"));
        connect(m_backup, &QCheckBox::toggled, this, [this](bool on) {
            m_user->saveSettingBool("AssetSDKFixerSaveBackups", on);
        });
        lay->addWidget(m_backup);

        auto *btnRow = new QHBoxLayout();
        auto *fix = new QPushButton("Localize", this);
        connect(fix, &QPushButton::clicked, this, &AssetFixerWindow::localize);
        btnRow->addStretch(1);
        btnRow->addWidget(fix);
        lay->addLayout(btnRow);
        m_status = new QLabel(this);
        lay->addWidget(m_status);
    }

private:
    void localize()
    {
        QString path = m_path->text().trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            QMessageBox::warning(this, "Asset Fixer - Error", "Open a place or model first.");
            return;
        }
        // LocalizeAsset parity: decompress .bz2 inputs, keep .bak backups.
        bool compressed = false;
        if (path.contains(".bz2")) {
            const QString out = path.left(path.size() - 4);
            if (QFileInfo::exists(out)) QFile::remove(out);
            QProcess bz;
            bz.start("bunzip2", {"-c", path});
            bz.waitForFinished(120000);
            QFile f(out);
            if (!f.open(QIODevice::WriteOnly)) {
                QMessageBox::warning(this, "Asset Fixer - Error", "Cannot decompress the map.");
                return;
            }
            f.write(bz.readAllStandardOutput());
            f.close();
            path = out;
            compressed = true;
        }

        bool binary = false;
        {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!f.atEnd()) {
                    if (QString::fromUtf8(f.readLine()).contains("<roblox!")) {
                        binary = true;
                        break;
                    }
                }
            }
        }
        if (binary) {
            if (compressed) QFile::remove(path);
            QMessageBox::warning(this, "Asset Fixer - Error",
                "Error: Unable to load the asset. Cannot load models/places in binary format.\n\n"
                "If the asset is a modern place or model, try converting the place or model to "
                "rbxlx/rbxmx format using MODERN Roblox Studio, then convert it using the Roblox "
                "Legacy Place Converter.");
            return;
        }
        if (m_backup->isChecked() && !QFileInfo::exists(path + ".bak")) {
            QFile::copy(path, path + ".bak");
        }

        const QString cacheDir = Paths::dataPath + "/assetcache/assets";
        QDir().mkpath(cacheDir);

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Asset Fixer - Error", "Cannot read " + path);
            return;
        }
        const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
        f.close();

        static const QRegularExpression urlRe("https?://[^\\s\"'<]+");
        const QString gameDir = "rbxasset://../../assets/";
        QStringList out = lines;
        int fixed_ = 0, errors = 0;
        QProgressDialog progress("Fixing assets...", QString(), 0, lines.size(), this);
        progress.setWindowModality(Qt::WindowModal);

        QNetworkAccessManager nam;
        for (int i = 0; i < lines.size(); i++) {
            progress.setValue(i);
            if (lines[i].contains("www.w3.org") || lines[i].contains("roblox.xsd")) continue;
            const auto matches = urlRe.globalMatch(lines[i]);
            for (const QRegularExpressionMatch &m : matches) {
                QString url = m.captured();
                if (url.contains(".png") || url.contains(".jpg") || url.contains(".jpeg")
                    || url.contains("my-roblox-character-item")) {
                    continue;
                }
                // FixURLString parity: normalize the URL onto assetdelivery.
                const int idx = url.indexOf("id=");
                if (idx < 0) continue;
                const QString id = url.mid(idx + 3).split('&').value(0);
                if (id.isEmpty() || !id.toLongLong()) continue;
                QFile cached(cacheDir + "/" + id);
                if (!cached.exists()) {
                    QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(
                        "https://assetdelivery.roblox.com/v1/asset/?id=" + id)));
                    QEventLoop loop;
                    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
                    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                    loop.exec();
                    if (reply->error() == QNetworkReply::NoError && reply->size() > 0) {
                        if (cached.open(QIODevice::WriteOnly)) {
                            cached.write(reply->readAll());
                            cached.close();
                        }
                    } else {
                        errors++;
                    }
                    reply->deleteLater();
                }
                if (cached.exists()) {
                    out[i].replace(url, gameDir + id);
                    fixed_++;
                }
            }
        }
        progress.setValue(lines.size());
        if (compressed) {
            QProcess bz;
            bz.start("bzip2", {"-c", path});
            bz.waitForFinished(120000);
            QFile bz2(path + ".bz2");
            if (bz2.open(QIODevice::WriteOnly)) {
                bz2.write(bz.readAllStandardOutput());
                bz2.close();
            }
            QFile::remove(path);
        }
        QFile out_(path);
        if (!out_.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Asset Fixer - Error", "Cannot write " + path);
            return;
        }
        out_.write(out.join("\n").toUtf8());
        out_.close();
        m_status->setText(QString::number(fixed_) + " URLs localized, "
                          + QString::number(errors) + " errors.");
    }

    Config *m_user;
    QLineEdit *m_path = nullptr;
    QCheckBox *m_backup = nullptr;
    QLabel *m_status = nullptr;
};

// ---- Clientinfo Editor (ClientinfoEditor.cs) -------------------------------
class ClientinfoEditorWindow : public QDialog
{
public:
    explicit ClientinfoEditorWindow(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle("Clientinfo Editor");
        setFixedWidth(560);
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 12);
        lay->setSpacing(10);

        auto *form = new QFormLayout();
        form->setSpacing(8);
        m_client = new QComboBox(this);
        QDir clientDir(Paths::clientDir);
        m_client->addItems(clientDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name));
        form->addRow("Client:", m_client);
        m_warning = new QLineEdit(this);
        m_description = new QLineEdit(this);
        m_exeName = new QLineEdit(this);
        m_cmdArgs = new QLineEdit(this);
        m_launchTime = new QDoubleSpinBox(this);
        m_launchTime->setRange(0.0, 10.0);
        m_launchTime->setSingleStep(0.25);
        m_revision = new QSpinBox(this);
        m_revision->setRange(0, 999);
        m_loadOptions = new QComboBox(this);
        m_loadOptions->addItems({
            "Client_2007_NoGraphicsOptions", "Client_2007", "Client_2008AndUp",
            "Client_2008AndUp_LegacyOpenGL", "Client_2008AndUp_QualityLevel21",
            "Client_2008AndUp_NoGraphicsOptions", "Client_2008AndUp_ForceAutomatic",
            "Client_2008AndUp_ForceAutomaticQL21",
            "Client_2008AndUp_HasCharacterOnlyShadowsLegacyOpenGL"});
        form->addRow("Warning:", m_warning);
        form->addRow("Description:", m_description);
        form->addRow("Custom EXE name:", m_exeName);
        form->addRow("Command line args:", m_cmdArgs);
        form->addRow("Client launch time:", m_launchTime);
        form->addRow("Client info revision:", m_revision);
        form->addRow("Client load options:", m_loadOptions);
        lay->addLayout(form);

        auto *flags = new QHBoxLayout();
        for (const char *n : {"Uses Player Name", "Uses ID", "Legacy Mode",
                              "Fix 2007", "Already Has Security", "Separate Folders",
                              "Custom EXE Name"}) {
            QCheckBox *box = new QCheckBox(QString::fromLatin1(n), this);
            m_flags << box;
            flags->addWidget(box);
        }
        lay->addLayout(flags);

        auto *row = new QHBoxLayout();
        auto *load = new QPushButton("Load", this);
        connect(load, &QPushButton::clicked, this, &ClientinfoEditorWindow::loadClient);
        auto *save = new QPushButton("Save", this);
        connect(save, &QPushButton::clicked, this, &ClientinfoEditorWindow::saveClient);
        row->addWidget(load);
        row->addWidget(save);
        row->addStretch(1);
        lay->addLayout(row);
        m_status = new QLabel(this);
        lay->addWidget(m_status);
        loadClient();
    }

private:
    QString clientFilePath() const
    {
        return Paths::clientDir + "/" + m_client->currentText() + "/clientinfo.nov";
    }

    void loadClient()
    {
        ClientInfo info;
        if (!ClientInfoFile::load(Paths::clientDir, m_client->currentText(), &info)) {
            m_status->setText("No clientinfo.nov for this client yet; defaults shown.");
            return;
        }
        m_warning->setText(info.warning);
        m_description->setText(info.description);
        m_exeName->setText(info.customClientEXEName);
        m_cmdArgs->setText(info.commandLineArgs);
        m_launchTime->setValue(info.clientLaunchTime);
        m_revision->setValue(info.clientInfoRevision);
        if (info.clientLoadOptions >= 0 && info.clientLoadOptions < m_loadOptions->count()) {
            m_loadOptions->setCurrentIndex(info.clientLoadOptions);
        }
        if (m_flags.size() >= 6) {
            m_flags[0]->setChecked(info.usesPlayerName);
            m_flags[1]->setChecked(info.usesID);
            m_flags[2]->setChecked(info.legacyMode);
            m_flags[3]->setChecked(info.fix2007);
            m_flags[4]->setChecked(info.alreadyHasSecurity);
            m_flags[5]->setChecked(info.seperateFolders);
        }
        if (m_flags.size() >= 7) {
            m_flags[6]->setChecked(info.usesCustomClientEXEName);
        }
        m_status->setText("Loaded.");
    }

    void saveClient()
    {
        // File.WriteAllText(clientinfo.nov, Encode(Encode(f0)|...|Encode(f16)))
        const auto enc = [](bool b) { return Crypto::encode(b ? "True" : "False"); };
        const auto encs = [](const QString &s) { return Crypto::encode(s); };
        ClientInfo info;
        ClientInfoFile::load(Paths::clientDir, m_client->currentText(), &info);
        QString md5 = info.clientMD5;
        // Recompute the client exe MD5 when a custom name is in use.
        const QString exeName = m_exeName->text().trimmed();
        if (!exeName.isEmpty()) {
            const QString exePath = Paths::clientDir + "/" + m_client->currentText() + "/" + exeName;
            if (QFileInfo::exists(exePath)) {
                QCryptographicHash hash(QCryptographicHash::Md5);
                QFile f(exePath);
                if (f.open(QIODevice::ReadOnly)) {
                    hash.addData(&f);
                    f.close();
                }
                md5 = QString::fromLatin1(hash.result().toHex()).toUpper();
            }
        }
        QStringList fields;
        fields << enc(m_flags[0]->isChecked())
               << enc(m_flags[1]->isChecked())
               << encs(m_warning->text())
               << enc(m_flags[2]->isChecked())
               << encs(md5)
               << encs("null")
               << encs(m_description->text())
               << enc("False")              // pre-generated marker
               << enc(m_flags[3]->isChecked())
               << enc(m_flags[4]->isChecked())
               << encs(QString::number(m_loadOptions->currentIndex()))
               << enc(m_flags[5]->isChecked())
               << enc(m_flags[6]->isChecked())
               << encs(exeName)
               << encs(m_cmdArgs->text())
               << encs(QString::number(m_launchTime->value()))
               << encs(QString::number(m_revision->value()));
        QFile f(clientFilePath());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::warning(this, "Clientinfo Editor - Error",
                                 "Cannot write " + clientFilePath());
            return;
        }
        f.write(Crypto::encode(fields.join("|")).toUtf8());
        f.close();
        m_status->setText("Saved " + clientFilePath());
    }

    QComboBox *m_client = nullptr;
    QLineEdit *m_warning = nullptr, *m_description = nullptr, *m_exeName = nullptr,
              *m_cmdArgs = nullptr;
    QDoubleSpinBox *m_launchTime = nullptr;
    QSpinBox *m_revision = nullptr;
    QComboBox *m_loadOptions = nullptr;
    QVector<QCheckBox *> m_flags;
    QLabel *m_status = nullptr;
};

// ---- Mod Creator (ModCreator.cs essentials) ---------------------------------
class ModCreatorWindow : public QDialog
{
public:
    ModCreatorWindow(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle("Mod Creator");
        setFixedWidth(520);
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 12);
        lay->setSpacing(10);
        auto *form = new QFormLayout();
        form->setSpacing(8);
        m_name = new QLineEdit(this);
        m_author = new QLineEdit(this);
        m_desc = new QLineEdit(this);
        form->addRow("Mod name:", m_name);
        form->addRow("Author:", m_author);
        form->addRow("Description:", m_desc);
        lay->addLayout(form);
        auto *addBtn = new QPushButton("Add files (paths relative to the data folder)...", this);
        connect(addBtn, &QPushButton::clicked, this, [this] {
            const QStringList files = QFileDialog::getOpenFileNames(
                this, "Select files to package", Paths::basePath);
            for (const QString &f : files) {
                const QString rel = QDir(Paths::basePath).relativeFilePath(f);
                if (!rel.startsWith("..")) {
                    m_files->addItem(rel);
                } else {
                    m_files->addItem(f);
                }
            }
        });
        lay->addWidget(addBtn);
        m_files = new QListWidget(this);
        m_files->setMinimumHeight(140);
        lay->addWidget(m_files);
        auto *del = new QPushButton("Remove selected", this);
        connect(del, &QPushButton::clicked, this, [this] {
            qDeleteAll(m_files->selectedItems());
        });
        lay->addWidget(del);
        auto *create = new QPushButton("Create package...", this);
        connect(create, &QPushButton::clicked, this, [this] {
            if (m_files->count() == 0) {
                QMessageBox::warning(this, "Novetus - Mod Creator", "Add at least one file.");
                return;
            }
            const QString dest = QFileDialog::getExistingDirectory(
                this, "Select the output folder", QDir::homePath());
            if (dest.isEmpty()) return;
            QStringList files;
            for (int i = 0; i < m_files->count(); i++) {
                const QString text = m_files->item(i)->text();
                files << (QFileInfo::exists(Paths::basePath + "/" + text)
                            ? text : QDir(Paths::basePath).relativeFilePath(text));
            }
            const QString outcome = ModManager::createModPackage(
                files, m_name->text().trimmed(), m_author->text().trimmed(),
                m_desc->text().trimmed(), dest);
            QMessageBox::information(this, "Novetus - Mod Creator", outcome);
            m_files->clear();
        });
        lay->addWidget(create);
    }

private:
    QLineEdit *m_name = nullptr, *m_author = nullptr, *m_desc = nullptr;
    QListWidget *m_files = nullptr;
};

// ---- Mesh Converter (MeshConverter.cs — wine-wrapped ObjToRBXMesh.exe) ------
class MeshConverterWindow : public QDialog
{
public:
    MeshConverterWindow(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle("Mesh Converter");
        setFixedWidth(460);
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 12);
        lay->setSpacing(10);
        lay->addWidget(new QLabel(
            "Converts an .obj mesh into the Roblox legacy .mesh format using "
            "ObjToRBXMesh.exe (runs under Wine when present in the data folder).", this));
        auto *row = new QHBoxLayout();
        m_path = new QLineEdit(this);
        auto *browse = new QPushButton("Browse...", this);
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString f = QFileDialog::getOpenFileName(this, "Open .obj",
                                                           QDir::homePath(), "Wavefront OBJ (*.obj)");
            if (!f.isEmpty()) m_path->setText(f);
        });
        row->addWidget(new QLabel("OBJ:", this));
        row->addWidget(m_path, 1);
        row->addWidget(browse);
        lay->addLayout(row);
        auto *verRow = new QHBoxLayout();
        verRow->addWidget(new QLabel("Mesh version:", this));
        m_version = new QComboBox(this);
        m_version->addItems({"0.2.11.283", "2.0", "2.1"});
        verRow->addWidget(m_version);
        verRow->addStretch(1);
        lay->addLayout(verRow);
        auto *go = new QPushButton("Convert", this);
        connect(go, &QPushButton::clicked, this, [this] {
            const QString exe = Paths::dataDir + "/ObjToRBXMesh.exe";
            if (!QFileInfo::exists(exe)) {
                QMessageBox::warning(this, "Mesh Converter - Error",
                    "ObjToRBXMesh.exe was not found in " + Paths::dataDir
                    + ". Place the tool there to convert meshes.");
                return;
            }
            QProcess p;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("WINEPREFIX", LinuxCompat::winePrefix());
            p.setProcessEnvironment(env);
            p.start(LinuxCompat::wineCommandProgram(),
                    QStringList(LinuxCompat::wineCommandPrefixArgs())
                        << QStringList{exe, m_path->text(), m_version->currentText()});
            p.waitForFinished(120000);
            QMessageBox::information(this, "Mesh Converter",
                p.exitCode() == 0 ? "Conversion finished." :
                "Conversion failed:\n" + QString::fromUtf8(p.readAllStandardError()));
        });
        lay->addWidget(go);
    }

private:
    QLineEdit *m_path = nullptr;
    QComboBox *m_version = nullptr;
};

// ---- Diogenes Editor (DiogenesEditor.cs) ------------------------------------
class DiogenesEditorWindow : public QDialog
{
public:
    DiogenesEditorWindow(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle("Diogenes Editor");
        resize(560, 420);
        auto *lay = new QVBoxLayout(this);
        m_versionLabel = new QLabel("Not Loaded", this);
        lay->addWidget(m_versionLabel);
        m_text = new QPlainTextEdit(this);
        lay->addWidget(m_text, 1);
        auto *row = new QHBoxLayout();
        auto *load = new QPushButton("Load diogenes.fnt", this);
        connect(load, &QPushButton::clicked, this, &DiogenesEditorWindow::load);
        auto *saveBtn = new QPushButton("Save (v2)", this);
        connect(saveBtn, &QPushButton::clicked, this, [this] { saveFile(true); });
        auto *saveV1 = new QPushButton("Save (v1)", this);
        connect(saveV1, &QPushButton::clicked, this, [this] { saveFile(false); });
        row->addWidget(load);
        row->addWidget(saveBtn);
        row->addWidget(saveV1);
        row->addStretch(1);
        lay->addLayout(row);
    }

private:
    // Util.CryptStringWithByte: XOR every ASCII byte with 0x55.
    static QString cryptLine(const QString &line)
    {
        const QByteArray bytes = line.toLatin1();
        QString out;
        for (char c : bytes) {
            out += QChar(uchar(0x55 ^ uchar(c)));
        }
        return out;
    }

    void load()
    {
        const QString f = QFileDialog::getOpenFileName(
            this, "Load diogenes.fnt", QDir::homePath(), "Roblox Diogenes filter (*.fnt)");
        if (f.isEmpty()) return;
        QFile file(f);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QStringList lines;
        m_version = 1;
        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.isEmpty()) continue;
            if (m_version == 1) {
                line = cryptLine(line);
                m_version = 2;
            }
            lines << line;
        }
        m_text->setPlainText(lines.join("\n"));
        m_versionLabel->setText("v" + QString::number(m_version));
    }

    void saveFile(bool v2)
    {
        const QString f = QFileDialog::getSaveFileName(
            this, "Save diogenes.fnt", "diogenes.fnt", "Roblox Diogenes filter (*.fnt)");
        if (f.isEmpty()) return;
        QFile file(f);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        const QStringList lines = m_text->toPlainText().split('\n');
        for (const QString &line : lines) {
            if (line.trimmed().isEmpty()) continue;
            file.write((v2 ? cryptLine(line) : line).toUtf8() + "\n");
        }
        m_versionLabel->setText(v2 ? "v2" : "v1");
    }

    QPlainTextEdit *m_text = nullptr;
    QLabel *m_versionLabel = nullptr;
    int m_version = 1;
};

// ---- Item Creation SDK (ItemCreationSDK.cs essentials) ----------------------
class ItemCreationWindow : public QDialog
{
public:
    ItemCreationWindow(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle("Novetus Item Creation SDK");
        setFixedWidth(520);
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 12);
        lay->setSpacing(10);
        auto *form = new QFormLayout();
        form->setSpacing(8);
        m_type = new QComboBox(this);
        m_type->addItems({"Hat", "Head", "Head (no custom mesh)", "Face", "T-Shirt", "Shirt", "Pants"});
        m_name = new QLineEdit(this);
        m_tex = new QLineEdit(this);
        m_mesh = new QLineEdit(this);
        m_desc = new QLineEdit(this);
        form->addRow("Item type:", m_type);
        form->addRow("Item name:", m_name);
        form->addRow("Texture file:", m_tex);
        form->addRow("Mesh file (Hat/Head):", m_mesh);
        form->addRow("Description:", m_desc);
        lay->addLayout(form);
        auto *browse = new QPushButton("Browse for texture/mesh files...", this);
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString f = QFileDialog::getOpenFileName(
                this, "Open texture or mesh", QDir::homePath(),
                "Images and meshes (*.png *.mesh)");
            if (f.isEmpty()) return;
            if (f.endsWith(".mesh")) m_mesh->setText(f);
            else m_tex->setText(f);
        });
        lay->addWidget(browse);
        auto *go = new QPushButton("Create Item", this);
        connect(go, &QPushButton::clicked, this, &ItemCreationWindow::create);
        lay->addWidget(go);
        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        lay->addWidget(m_status);
        updateTemplate();
        connect(m_type, &QComboBox::currentIndexChanged, this, [this] { updateTemplate(); });
    }

private:
    QString templateFor(int type) const
    {
        static const char *names[] = {
            "HatTemplate.rbxm", "HeadTemplate.rbxm", "HeadNoCustomMeshTemplate.rbxm",
            "FaceTemplate.rbxm", "TShirtTemplate.rbxm", "ShirtTemplate.rbxm",
            "PantsTemplate.rbxm"};
        return Paths::configDirTemplates + "/" + names[type];
    }

    // Per-type Content property + in-game path (RobloxDefs parity).
    struct ItemDef {
        const char *cls;      // XML item class
        const char *prop;     // Content property name
        const char *outDir;   // local output dir
        const char *gameDir;  // in-game path prefix
    };
    ItemDef defFor(int type) const
    {
        switch (type) {
        case 0: return {"SpecialMesh", "MeshId", "hats", "rbxasset://../../charcustom/hats/"};
        case 1: return {"SpecialMesh", "TextureId", "heads", "rbxasset://../../charcustom/heads/"};
        case 2: return {"SpecialMesh", "TextureId", "heads", "rbxasset://../../charcustom/heads/"};
        case 3: return {"Decal", "Texture", "faces", "rbxasset://../../charcustom/faces/"};
        case 4: return {"ShirtGraphic", "Graphic", "tshirts", "rbxasset://../../charcustom/tshirts/"};
        case 5: return {"Shirt", "ShirtTemplate", "shirts", "rbxasset://../../charcustom/shirts/"};
        default: return {"Pants", "PantsTemplate", "pants", "rbxasset://../../charcustom/pants/"};
        }
    }

    void updateTemplate()
    {
        const QString tmpl = templateFor(m_type->currentIndex());
        m_status->setText("Template: " + tmpl
                          + (QFileInfo::exists(tmpl) ? "" : " (missing!)"));
    }

    void create()
    {
        const QString name = m_name->text().trimmed().remove(' ');
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Novetus Item Creation SDK", "Enter an item name.");
            return;
        }
        const QString tmpl = templateFor(m_type->currentIndex());
        if (!QFileInfo::exists(tmpl)) {
            QMessageBox::warning(this, "Novetus Item Creation SDK",
                                 "Template not found: " + tmpl);
            return;
        }
        QFile t(tmpl);
        if (!t.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        const QString xml = QString::fromUtf8(t.readAll());
        t.close();

        const ItemDef def = defFor(m_type->currentIndex());
        const QString outDir = Paths::dataPath + "/charcustom/" + def.outDir;
        QDir().mkpath(outDir);
        const bool hasMesh = m_type->currentIndex() <= 2; // Hat/Head types

        // Copy the assets in (SetItemFontVals' File.Copy parity).
        QString texName = name + ".png";
        QString meshName = name + ".mesh";
        if (!m_tex->text().trimmed().isEmpty()) {
            QFile::remove(outDir + "/" + texName);
            QFile::copy(m_tex->text().trimmed(), outDir + "/" + texName);
        }
        if (hasMesh && !m_mesh->text().trimmed().isEmpty()) {
            QFile::remove(outDir + "/" + meshName);
            QFile::copy(m_mesh->text().trimmed(), outDir + "/" + meshName);
        }

        QString modified = xml;
        // Content url parity: the mesh property points at fonts/, the texture
        // property at textures/ inside the category's game directory.
        const QString texUrl = QString(def.gameDir) + "textures/" + texName;
        const QString meshUrl = QString(def.gameDir) + "fonts/" + meshName;
        const QRegularExpression urlRe("<url>[^<]*</url>");
        QRegularExpressionMatchIterator it = urlRe.globalMatch(modified);
        QStringList replacedUrls;
        const QString firstNew = hasMesh ? meshUrl : texUrl;
        int index = 0;
        while (it.hasNext()) {
            replacedUrls << (index == 0 ? firstNew : (hasMesh ? texUrl : QString()));
            index++;
        }
        int replacement = 0;
        QString result;
        qsizetype last = 0;
        it = urlRe.globalMatch(modified);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            result += modified.mid(last, m.capturedStart() - last);
            result += "<url>" + replacedUrls[replacement] + "</url>";
            last = m.capturedEnd();
            replacement++;
        }
        result += modified.mid(last);
        modified = result;

        const QString outPath = outDir + "/" + name + ".rbxm";
        QFile out(outPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::warning(this, "Novetus Item Creation SDK",
                                 "Cannot write " + outPath);
            return;
        }
        out.write(modified.toUtf8());
        out.close();

        if (!m_desc->text().trimmed().isEmpty()) {
            QFile d(outDir + "/" + name + "_desc.txt");
            if (d.open(QIODevice::WriteOnly | QIODevice::Text)) {
                d.write(m_desc->text().trimmed().toUtf8());
            }
        }
        QMessageBox::information(this, "Novetus Item Creation SDK - Item Creation Success",
            "The creation of your item, " + m_name->text().trimmed()
            + ", is successful! It is now available in Character Customization (" + def.outDir + ").");
    }

    QComboBox *m_type = nullptr;
    QLineEdit *m_name = nullptr, *m_tex = nullptr, *m_mesh = nullptr, *m_desc = nullptr;
    QLabel *m_status = nullptr;
};

// ---- SDK hub ----------------------------------------------------------------
SdkWindow::SdkWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Novetus SDK");
    setFixedSize(420, 380);
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 12);
    lay->setSpacing(10);
    lay->addWidget(new QLabel("Novetus SDK Tools", this));
    auto *list = new QListWidget(this);
    const QStringList tools = {
        "Clientinfo Editor", "Asset Fixer", "Asset Downloader", "Mesh Converter",
        "Item SDK", "Mod Creator", "Client Script Documentation", "Splash Tester",
        "Diogenes Editor"};
    list->addItems(tools);
    list->setCurrentRow(0);
    lay->addWidget(list, 1);
    auto *row = new QHBoxLayout();
    auto *open = new QPushButton("Open Tool", this);
    connect(open, &QPushButton::clicked, this, [this, list] { openTool(list->currentRow()); });
    connect(list, &QListWidget::itemDoubleClicked, this, [this, list](QListWidgetItem *) {
        openTool(list->currentRow());
    });
    row->addStretch(1);
    row->addWidget(open);
    lay->addLayout(row);
}

void SdkWindow::openTool(int index)
{
    GuiShared::Ui ui = GuiShared::createConfigs();
    switch (index) {
    case 0: {
        ClientinfoEditorWindow w(this);
        w.exec();
        break;
    }
    case 1: {
        AssetFixerWindow w(ui.user, this);
        w.exec();
        break;
    }
    case 2: {
        AssetDownloaderWindow w(ui.user, this);
        w.exec();
        break;
    }
    case 3: {
        MeshConverterWindow w(this);
        w.exec();
        break;
    }
    case 4: {
        ItemCreationWindow w(this);
        w.exec();
        break;
    }
    case 5: {
        ModCreatorWindow w(this);
        w.exec();
        break;
    }
    case 6: {
        DocsWindow w(this);
        w.exec();
        break;
    }
    case 7: {
        SplashTesterWindow w(ui.user, this);
        w.exec();
        break;
    }
    case 8: {
        DiogenesEditorWindow w(this);
        w.exec();
        break;
    }
    }
}
