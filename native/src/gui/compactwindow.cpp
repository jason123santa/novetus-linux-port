#include "compactwindow.h"

#include <QTabWidget>
#include <QRandomGenerator>
#include <algorithm>
#include "core/launcher.h"
#include "fit.h"
#include "customizationwindow.h"
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QProcess>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QFont>

#include "serverbrowser.h"
#include "settingsdialog.h"
#include "core/paths.h"
#include "sdkwindow.h"
#include "core/modmanager.h"
#include "core/filemanagement.h"


// --- shared page builders used by both Compact and the Extended-style pages ---
namespace {

QLabel *lbl(QWidget *p, int x, int y, int w, int h, const QString &t, float pt = 8.25f,
            Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *l = new QLabel(t, p);
    l->setGeometry(x, y, w, h);
    l->setAlignment(a);
    l->setFont(QFont("Microsoft Sans Serif", pt));
    return l;
}

QLineEdit *line(QWidget *p, int x, int y, int w, int h, const QString &t = {})
{
    auto *e = new QLineEdit(p);
    e->setGeometry(x, y, w, h);
    e->setText(t);
    e->setFont(QFont("Microsoft Sans Serif", 8.25f));
    return e;
}

QPlainTextEdit *edit(QWidget *p, int x, int y, int w, int h)
{
    auto *e = new QPlainTextEdit(p);
    e->setGeometry(x, y, w, h);
    e->setFont(QFont("Microsoft Sans Serif", 8.25f));
    return e;
}

QPushButton *btn(QWidget *p, int x, int y, int w, int h, const QString &t, float pt,
                 std::function<void()> fn)
{
    auto *b = new QPushButton(t, p);
    b->setGeometry(x, y, w, h);
    b->setFont(QFont("Microsoft Sans Serif", pt));
    if (fn) QObject::connect(b, &QPushButton::clicked, fn);
    return b;
}

QFrame *rect(QWidget *p, int x, int y, int w, int h, const char *fill, const char *stroke)
{
    auto *f = new QFrame(p);
    f->setGeometry(x, y, w, h);
    f->setStyleSheet(QString("background: %1; border: 1px solid %2;").arg(fill, stroke));
    return f;
}

} // namespace

CompactWindow::CompactWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Novetus");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    m_ui = GuiShared::createConfigs();

    QWidget *central = new QWidget(this);
    central->setFixedSize(417, 366);
    setCentralWidget(central);
    setFixedSize(417, 366);

    // Header
    QLabel *logo = new QLabel(central);
    logo->setPixmap(QPixmap(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png")
                        .scaled(51, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setGeometry(2, 12, 51, 50);
    lbl(central, 55, 4, 109, 14, "SELECTED CLIENT:");
    m_selectedClient = lbl(central, 160, 4, 245, 13, m_ui.user->readSetting("SelectedClient"));
    lbl(central, 55, 18, 95, 12, "SELECTED MAP:");
    m_selectedMap = lbl(central, 145, 18, 260, 12, m_ui.user->readSetting("Map"), 6.75f);
    lbl(central, 55, 34, 40, 16, "Name:");
    auto *nameBox = line(central, 95, 31, 121, 20, m_ui.user->readSetting("PlayerName", true));
    connect(nameBox, &QLineEdit::editingFinished, this, [this, nameBox] {
        m_ui.user->saveSetting("PlayerName", nameBox->text(), true);
    });
    lbl(central, 55, 55, 34, 13, "ID:");
    auto *idBox = line(central, 95, 53, 71, 18,
                       QString::number(m_ui.user->readSettingInt("UserID")));
    idBox->setAlignment(Qt::AlignRight);
    connect(idBox, &QLineEdit::editingFinished, this, [this, idBox] {
        m_ui.user->saveSettingInt("UserID", idBox->text().toInt());
    });
    btn(central, 169, 52, 48, 20, "Regen", 6.75f, [this, idBox] {
        const int id = QRandomGenerator::global()->bounded(99999999);
        m_ui.user->saveSettingInt("UserID", id);
        idBox->setText(QString::number(id));
    });
    btn(central, 223, 31, 47, 20, "Avatar", 6.75f, [this] {
        CustomizationWindow w(this);
        w.exec();
    });
    btn(central, 271, 31, 43, 20, "Studio", 6.75f, [this] {
        GuiShared::launchGame(m_ui, ScriptType_Studio, false, this);
    });
    btn(central, 315, 31, 63, 20, "Install Mod", 6.75f, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, "Open a mod package or addon script", QDir::homePath(),
            "Novetus Mods (*.zip *.novmod *.lua);;Mod Package (*.zip *.novmod);;Addon Script (*.lua)");
        if (file.isEmpty()) return;
        ModManager::InstallResult result;
        if (file.endsWith(".lua", Qt::CaseInsensitive)) {
            result = ModManager::installAddonScript(file);
        } else {
            result = ModManager::installModPackage(file);
        }
        if (!result.outcome.isEmpty()) {
            QMessageBox::information(this, result.ok ? "Novetus - Mod Installed" : "Novetus - Mod Error",
                                     result.outcome);
        }
    });
    btn(central, 380, 31, 35, 20, "SDK", 6.75f, [this] {
        SdkWindow w(this);
        w.exec();
    });
    // Splash (label12: 223,51,192x21) — overlapped by the buttons row; text below
    m_splash = lbl(central, 223, 55, 192, 21, GuiShared::pickSplash(m_ui.user), 6.75f,
                   Qt::AlignCenter);

    // Tabbed content (tabControl1: 2,79,413x284) — real visible tabs
    auto *tabs = new QTabWidget(central);
    tabs->setGeometry(2, 79, 413, 284);
    tabs->setFont(QFont("Microsoft Sans Serif", 8.25f));
    tabs->setStyleSheet("QTabBar::tab { font-size: 8pt; padding: 2px 5px; }");

    // PLAY
    auto *play = new QWidget();
    lbl(play, 67, 5, 99, 13, "Server Address");
    auto *address = line(play, 6, 21, 223, 20, "localhost:53640");
    const QString ip = m_ui.user->readSetting("IP");
    if (!ip.isEmpty() && ip != "N/A") {
        address->setText(ip + ":" + m_ui.user->readSetting("RobloxPort"));
    }
    btn(play, 6, 47, 223, 19, "JOIN SERVER FROM IP", 8.25f, [this, address] {
        GuiShared::launchGame(m_ui, ScriptType_Client, false, this);
    });
    btn(play, 235, 47, 164, 19, "BROWSE SERVERS", 8.25f, [this] {
        ServerBrowser browser(this);
browser.exec();
    });
    btn(play, 106, 72, 192, 29, "PLAY SOLO", 8.25f, [this] {
        GuiShared::launchGame(m_ui, ScriptType_Solo, false, this);
    });
    auto *sep1 = new QLabel(play); sep1->setGeometry(6, 105, 393, 2); sep1->setFrameShape(QFrame::Box);
    QLabel *note = lbl(play, 6, 107, 393, 136,
        "NOTE: \n- If you have a problem where you can't see your character, REGENERATE YOUR "
        "PLAYER ID THEN REJOIN THE SERVER. \n- If you tried to connect to the server and you "
        "get an error, REJOIN THE SERVER.\n- To reset your character, type !!reset into chat.",
        6.75f, Qt::AlignCenter);
    note->setWordWrap(true);
    note->setStyleSheet("color: red; font-weight: bold;");
    tabs->addTab(play, "PLAY");

    // HOST
    auto *host = new QWidget();
    lbl(host, 6, 5, 129, 13, "SERVER INFORMATION");
    auto *info = edit(host, 6, 21, 393, 96);
    info->setReadOnly(true);
    auto *sep2 = new QLabel(host); sep2->setGeometry(6, 158, 393, 2); sep2->setFrameShape(QFrame::Box);
    lbl(host, 52, 165, 65, 16, "Server Port");
    auto *port = new QSpinBox(host);
    port->setGeometry(123, 163, 63, 20);
    port->setRange(1, 65535);
    port->setValue(m_ui.user->readSettingInt("RobloxPort"));
    lbl(host, 199, 165, 60, 16, "Player Limit");
    auto *limit = new QSpinBox(host);
    limit->setGeometry(265, 163, 77, 20);
    limit->setRange(1, 99);
    limit->setValue(m_ui.user->readSettingInt("PlayerLimit"));
    auto *upnp = new QCheckBox("UPnP", host);   upnp->setGeometry(228, 193, 54, 17);
    auto *notif = new QCheckBox("Notifications", host); notif->setGeometry(288, 193, 84, 17);
    Fit::row({upnp, notif}, 14);
    btn(host, 21, 189, 137, 22, "RESET PORT", 8.25f, [this, port] {
        port->setValue(53640);
        m_ui.user->saveSettingInt("RobloxPort", 53640);
    });
    lbl(host, 8, 138, 69, 13, "Server Name");
    line(host, 77, 135, 84, 20, m_ui.user->readSetting("ServerBrowserName"));
    lbl(host, 189, 132, 104, 24, "Master Server Address \n(Port Optional)", 6.75f);
    line(host, 299, 135, 100, 20, m_ui.user->readSetting("MasterServerAddress"));
    lbl(host, 120, 119, 164, 13, "SERVER BROWSER OPTIONS:");
    btn(host, 6, 216, 188, 36, "START SERVER", 8.25f, [this] {
        GuiShared::launchGame(m_ui, ScriptType_Server, false, this);
    });
    btn(host, 204, 216, 195, 35, "START SERVER WITH NO GRAPHICS", 8.25f, [this] {
        GuiShared::launchGame(m_ui, ScriptType_Server, false, this);
    });
    connect(port, &QSpinBox::valueChanged, this, [this](int v) {
        m_ui.user->saveSettingInt("RobloxPort", v);
    });
    connect(limit, &QSpinBox::valueChanged, this, [this](int v) {
        m_ui.user->saveSettingInt("PlayerLimit", v);
    });
    connect(upnp, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("UPnP", on);
    });
    connect(notif, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("ShowServerNotifications", on);
    });
    tabs->addTab(host, "HOST");

    // VERSIONS
    auto *versions = new QWidget();
    auto *clients = new QListWidget(versions);
    clients->setGeometry(6, 7, 393, 95);
    auto *desc = lbl(versions, 6, 105, 393, 71, "", 8.25f, Qt::AlignTop);
    desc->setWordWrap(true);
    auto *warn = edit(versions, 6, 179, 393, 73);
    warn->setReadOnly(true);
    Launcher l(ScriptContext{});
    clients->addItems(l.listClients());
    const int idx = clients->findItems(m_ui.user->readSetting("SelectedClient"),
                                       Qt::MatchExactly).count()
                            ? clients->row(clients->findItems(
                                   m_ui.user->readSetting("SelectedClient"),
                                   Qt::MatchExactly).first()) : -1;
    if (idx >= 0) clients->setCurrentRow(idx);
    connect(clients, &QListWidget::currentTextChanged, this, [this, desc, warn](const QString &c) {
        m_ui.user->saveSetting("SelectedClient", c);
        m_selectedClient->setText(c);
        ClientInfo info;
        if (ClientInfoFile::load(Paths::clientDir, c, &info)) {
            desc->setText(info.description);
            warn->setPlainText(info.warning);
        }
    });
    tabs->addTab(versions, "VERSIONS");

    // MAPS
    auto *maps = new QWidget();
    auto *search = line(maps, 7, 5, 125, 20);
    auto *searchBtn = new QPushButton(maps);
    searchBtn->setGeometry(141, 3, 26, 23);
    searchBtn->setIcon(QIcon(":/assets/search.png"));
    searchBtn->setFlat(true);
    auto *refreshBtn = new QPushButton(maps);
    refreshBtn->setGeometry(173, 3, 24, 23);
    refreshBtn->setIcon(QIcon(":/assets/refresh.png"));
    refreshBtn->setFlat(true);
    auto *tree = new QTreeWidget(maps);
    tree->setHeaderHidden(true);
    tree->setGeometry(7, 29, 392, 108);
    auto *mapDesc = edit(maps, 7, 143, 392, 110);
    mapDesc->setReadOnly(true);
    auto reloadMaps = [tree, search]() {
        tree->clear();
        auto *root = new QTreeWidgetItem(tree, {"maps"});
        QDir mapsDir(Paths::mapsDir);
        const QString f = search->text();
        const QRegularExpression re(
            f.isEmpty() ? QString() : QRegularExpression::wildcardToRegularExpression(f),
            QRegularExpression::CaseInsensitiveOption);
        std::function<void(QTreeWidgetItem *, const QDir &)> addDir =
            [&](QTreeWidgetItem *parent, const QDir &dir) {
                // TreeNodeHelper.CreateDirectoryNode parity: subdirectories
                // first (at any depth), then matching files
                for (const QString &sub :
                     dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
                    auto *cat = new QTreeWidgetItem(parent, {sub});
                    addDir(cat, QDir(dir.absoluteFilePath(sub)));
                }
                for (const QFileInfo &fi : dir.entryInfoList(QDir::Files, QDir::Name)) {
                    if (!fi.completeSuffix().startsWith("rbxl")) continue;
                    if (!f.isEmpty() && !re.match(fi.fileName()).hasMatch()) continue;
                    new QTreeWidgetItem(parent, {fi.fileName()});
                }
            };
        addDir(root, QDir(Paths::mapsDir));
        root->setExpanded(true);
    };
    reloadMaps();
    connect(searchBtn, &QPushButton::clicked, reloadMaps);
    connect(refreshBtn, &QPushButton::clicked, this, [this, reloadMaps, search] {
        search->clear();
        reloadMaps();
    });
    btn(maps, 219, 3, 75, 23, "Add Map...", 8.25f, [this, reloadMaps] {
        const QString file = QFileDialog::getOpenFileName(this, "Add Map", Paths::mapsDir,
            "Roblox Maps (*.rbxl *.rbxlx *.rbxl.bz2 *.rbxlx.bz2)");
        if (file.isEmpty()) return;
        QDir().mkpath(Paths::mapsDirCustom);
        QFile::copy(file, Paths::mapsDirCustom + "/" + QFileInfo(file).fileName());
        reloadMaps();
    });
    btn(maps, 297, 3, 102, 23, "Open Maps Folder", 6.75f, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::mapsDir));
    });
    connect(tree, &QTreeWidget::itemDoubleClicked, this,
            [this, mapDesc, tree](QTreeWidgetItem *it, int) {
        if (!it || !it->parent()) return;
        const QString map = it->text(0);
        QString rel;
        for (QTreeWidgetItem *p = it->parent(); p && p->parent(); p = p->parent()) {
            rel = p->text(0) + (rel.isEmpty() ? QString() : "/" + rel);
        }
        const QString path = Paths::mapsDir + "/" + rel + "/" + map;
        m_ui.user->saveSetting("Map", map);
        m_ui.user->saveSetting("MapPath", path);
        m_ui.user->saveSetting("MapPathSnip", "maps/" + rel + "/" + map);
        m_selectedMap->setText(map);
        mapDesc->setPlainText("Selected map: " + map + "\nPath: " + path);
        // Client.DecompressMap() parity: unpack compressed maps on selection
        if (map.endsWith(".bz2") && !QFileInfo::exists(path.left(path.size() - 4))) {
            QProcess bz;
            bz.start("/bin/sh", {"-c", "bunzip2 -c \"$1\" > \"$2\"", "sh", path,
                                 path.left(path.size() - 4)});
            bz.waitForFinished(120000);
        }
    });
    tabs->addTab(maps, "MAPS");

    // SAVED
    auto *saved = new QWidget();
    lbl(saved, 67, 3, 59, 15, "SERVERS");
    auto *servers = new QListWidget(saved);
    servers->setGeometry(6, 21, 186, 160);
    lbl(saved, 282, 3, 59, 15, "PORTS");
    auto *ports = new QListWidget(saved);
    ports->setGeometry(213, 21, 186, 160);
    auto reloadSaved = [servers, ports] {
        servers->clear();
        ports->clear();
        QFile s(Paths::configDir + "/servers.txt"), p(Paths::configDir + "/ports.txt");
        if (s.open(QIODevice::ReadOnly | QIODevice::Text)) {
            servers->addItems(QString::fromUtf8(s.readAll()).split("\n", Qt::SkipEmptyParts));
        }
        if (p.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ports->addItems(QString::fromUtf8(p.readAll()).split("\n", Qt::SkipEmptyParts));
        }
    };
    reloadSaved();
    auto append = [](const QString &path, const QString &v) {
        QFile f(path);
        if (f.open(QIODevice::Append | QIODevice::Text)) f.write((v + "\n").toUtf8());
    };
    btn(saved, 6, 186, 60, 23, "Add", 8.25f, [append, address, reloadSaved] {
        const int c = address->text().lastIndexOf(':');
        append(Paths::configDir + "/servers.txt",
               c > 0 ? address->text().left(c) : address->text());
        reloadSaved();
    });
    btn(saved, 69, 186, 62, 23, "Remove", 8.25f, [servers, reloadSaved] {
        QFile f(Paths::configDir + "/servers.txt");
        QStringList lines;
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            lines = QString::fromUtf8(f.readAll()).split("\n");
        }
        const int r = servers->currentRow();
        if (r >= 0 && r < lines.size()) lines.removeAt(r);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(lines.join("\n").toUtf8());
        reloadSaved();
    });
    btn(saved, 137, 186, 55, 23, "Reset", 8.25f, reloadSaved);
    btn(saved, 213, 186, 54, 23, "Add", 8.25f, [append, port, reloadSaved] {
        append(Paths::configDir + "/ports.txt", port->text());
        reloadSaved();
    });
    btn(saved, 273, 186, 60, 23, "Remove", 8.25f, [ports, reloadSaved] {
        QFile f(Paths::configDir + "/ports.txt");
        QStringList lines;
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            lines = QString::fromUtf8(f.readAll()).split("\n");
        }
        const int r = ports->currentRow();
        if (r >= 0 && r < lines.size()) lines.removeAt(r);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(lines.join("\n").toUtf8());
        reloadSaved();
    });
    btn(saved, 339, 186, 60, 23, "Reset", 8.25f, reloadSaved);
    lbl(saved, 6, 212, 136, 12, "CURRENT SERVER IP:");
    lbl(saved, 6, 226, 120, 20, ip.isEmpty() ? "localhost" : ip);
    lbl(saved, 213, 212, 149, 12, "CURRENT SERVER PORT:");
    lbl(saved, 213, 226, 120, 20, port->text());
    tabs->addTab(saved, "SAVED");

    // CHANGES
    auto *changes = new QWidget();
    auto *log = edit(changes, 3, 3, 399, 252);
    log->setReadOnly(true);
    QFile cl(Paths::basePath + "/changelog.txt");
    if (cl.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log->setPlainText(QString::fromUtf8(cl.readAll()));
    }
    tabs->addTab(changes, "CHANGES");

    // OPTIONS
    auto *options = new QWidget();
    auto *discord = new QCheckBox("Discord RPC", options);       discord->setGeometry(6, 3, 87, 17);
    auto *localPlay = new QCheckBox("Local Play", options);      localPlay->setGeometry(100, 2, 77, 19);
    auto *minimize = new QCheckBox("Minimize on Launch", options); minimize->setGeometry(186, 3, 127, 19);
    auto *webProxy = new QCheckBox("Web Proxy", options);        webProxy->setGeometry(319, 3, 78, 17);
    Fit::row({discord, localPlay, minimize, webProxy}, 12);
    btn(options, 6, 21, 88, 34, "Reset Config", 6.75f, [this] {
        if (QMessageBox::question(this, "Novetus - Reset",
                                  "Reset the launcher configuration? The launcher will restart.")
            != QMessageBox::Yes) {
            return;
        }
        FileManagement::resetConfigValues(m_ui.user, m_ui.user->readSettingInt("LauncherStyle"),
                                          m_ui.user->readSetting("SelectedClient"),
                                          m_ui.user->readSetting("Map"));
        FileManagement::resetCustomizationValues(m_ui.custom);
        qApp->quit();
    });
    btn(options, 96, 21, 48, 34, "Clear Cache", 6.75f, [this] {
        QDir(Paths::dataPath + "/assetcache").removeRecursively();
        QDir().mkpath(Paths::dataPath + "/assetcache");
    });
    btn(options, 146, 21, 61, 34, "Graphics Settings", 6.75f, [this] {
        SettingsDialog w(this);
        w.exec();
    });
    lbl(options, 211, 31, 78, 13, "Launcher Style");
    auto *style = new QComboBox(options);
    style->setGeometry(296, 28, 99, 21);
    style->addItems({"None", "Extended", "Compact", "Stylish"});
    style->setCurrentIndex(std::clamp(m_ui.user->readSettingInt("LauncherStyle"), 0, 3));
    connect(style, &QComboBox::activated, this, [this, style](int i) {
        m_ui.user->saveSettingInt("LauncherStyle", i);
        GuiShared::restartLauncher();
    });
    auto *sep3 = new QLabel(options); sep3->setGeometry(6, 60, 389, 2); sep3->setFrameShape(QFrame::Box);
    auto *credits = edit(options, 6, 70, 389, 132);
    credits->setReadOnly(true);
    QFile rm(Paths::basePath + "/README-AND-CREDITS.TXT");
    if (rm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        credits->setPlainText(QString::fromUtf8(rm.readAll()));
    }
    lbl(options, 6, 205, 389, 15, "v1.0", 9.75f);
    lbl(options, 6, 221, 389, 16,
        "Novetus is a Roblox multilauncher built for the community.");
    lbl(options, 8, 237, 182, 16, "EDGE Snapshot v26.0.2 (native C++ build)");
    lbl(options, 214, 237, 181, 16, "PROJECT STARLIGHT", 8.25f, Qt::AlignRight);
    discord->setChecked(m_ui.user->readSettingBool("DiscordRichPresence"));
    localPlay->setChecked(m_ui.user->readSettingBool("LocalPlay"));
    minimize->setChecked(m_ui.user->readSettingBool("CloseOnLaunch"));
    webProxy->setChecked(m_ui.user->readSettingBool("WebProxyEnabled"));
    connect(discord, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("DiscordRichPresence", on);
    });
    connect(localPlay, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("LocalPlay", on);
    });
    connect(minimize, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("CloseOnLaunch", on);
    });
    connect(webProxy, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("WebProxyEnabled", on);
    });
    tabs->addTab(options, "OPTIONS");
}
