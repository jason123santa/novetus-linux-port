#include "extendedwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QSpinBox>
#include <QDate>
#include <algorithm>
#include <functional>
#include <QApplication>

#include "serverbrowser.h"
#include "settingsdialog.h"
#include "sdkwindow.h"
#include "core/modmanager.h"
#include "core/filemanagement.h"
#include "core/linuxcompat.h"
#include "core/paths.h"
#include "core/crypto.h"
#include "fit.h"
#include "customizationwindow.h"
#include "core/config.h"

namespace {
QStringList readLines(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll()).split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
}

void appendLine(const QString &path, const QString &line)
{
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write((line + "\n").toUtf8());
    }
}
} // namespace

ExtendedWindow::ExtendedWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Novetus EDGE Snapshot v26.0.2 (native Linux)");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    m_userConfig = new Config(QStringLiteral("Config"), Paths::configDir + "/config.json");
    m_customConfig = new Config(QStringLiteral("Customization"),
                                Paths::configDir + "/config_customization.json");
    m_userConfig->load();
    m_customConfig->load();

    QWidget *central = new QWidget(this);
    // panels end at y=346; no blank strip below
    central->setFixedSize(739, 346);
    setCentralWidget(central);
    // FixedSingle + MaximizeBox=false parity: the launcher cannot be resized
    setFixedSize(739, 346);

    // ---- panel3: logo + user identity (1,4,229x69) ----
    QWidget *panel3 = makePanel(central, 1, 4, 229, 69);
    QLabel *logo = new QLabel(panel3);
    QPixmap n(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png");
    logo->setPixmap(n.scaled(43, 41, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setGeometry(3, 3, 43, 41);
    makeLabel(panel3, 57, 6, 40, 16, "Name:");
    m_nameBox = makeLine(panel3, 97, 3, 121, 20,
                         m_userConfig->readSetting("PlayerName", true));
    connect(m_nameBox, &QLineEdit::editingFinished, this, [this] {
        m_userConfig->saveSetting("PlayerName", m_nameBox->text(), true);
    });
    makeLabel(panel3, 57, 27, 34, 13, "ID:");
    m_idBox = makeLine(panel3, 97, 25, 71, 18,
                       QString::number(m_userConfig->readSettingInt("UserID")));
    m_idBox->setAlignment(Qt::AlignRight);
    connect(m_idBox, &QLineEdit::editingFinished, this, [this] {
        m_userConfig->saveSettingInt("UserID", m_idBox->text().toInt());
    });
    makeButton(panel3, 171, 24, 48, 20, "Regen", 6.75f, [this] {
        const int id = QRandomGenerator::global()->bounded(99999999);
        m_userConfig->saveSettingInt("UserID", id);
        m_idBox->setText(QString::number(id));
        logLine("Regenerated player ID.");
    });
    // vertical separator (label16)
    auto *sep = new QLabel(panel3);
    sep->setGeometry(52, 3, 2, 41);
    sep->setFrameShape(QFrame::Box);
    // splash (label12)
    m_splash = makeLabel(panel3, 4, 47, 214, 17, "", 6.75f,
                         Qt::AlignCenter);
    pickSplash();

    // ---- header right: selected client/map + top buttons ----
    makeLabel(central, 236, 10, 109, 14, "SELECTED CLIENT:");
    m_selectedClientLabel = makeLabel(central, 343, 10, 389, 13,
                                      m_userConfig->readSetting("SelectedClient"));
    makeLabel(central, 236, 24, 95, 12, "SELECTED MAP:");
    m_selectedMapLabel = makeLabel(central, 329, 25, 403, 18,
                                   m_userConfig->readSetting("Map"), 6.75f);

    QWidget *panel4 = makePanel(central, 236, 41, 501, 32);
    makeButton(panel4, 12, 3, 113, 20, "Customize Character", 6.75f, [this] {
        CustomizationWindow w(this);
        w.exec();
    });
    makeButton(panel4, 131, 3, 113, 20, "Launch Studio", 6.75f, [this] { doLaunch(ScriptType_Studio); });
    makeButton(panel4, 250, 3, 113, 20, "Novetus SDK", 6.75f, [this] {
        SdkWindow w(this);
        w.exec();
    });
    makeButton(panel4, 369, 3, 113, 20, "Install Mod Package", 6.75f, [this] {
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
            logLine(result.outcome);
        }
    });

    // ---- sidebar (panel1, 1,74,84x272) ----
    QWidget *panel1 = makePanel(central, 1, 74, 84, 272);
    const QStringList sideNames = {"PLAY", "HOST", "VERSIONS", "MAPS", "SAVED", "CHANGES", "OPTIONS"};
    for (int i = 0; i < sideNames.size(); i++) {
        QPushButton *b = new QPushButton(sideNames[i], panel1);
        b->setGeometry(3, 4 + i * 25, 75, 23);
        b->setCheckable(true);
        b->setFont(QFont("Microsoft Sans Serif", 8.25f));
        m_sideButtons << b;
        connect(b, &QPushButton::clicked, this, [this, i] {
            for (int j = 0; j < m_sideButtons.size(); j++) {
                m_sideButtons[j]->setChecked(j == i);
            }
            m_pages->setCurrentIndex(i);
        });
    }
    makeLabel(panel1, 3, 180, 75, 88,
              "v1.5\nwill never\nbe a\nthing", 6.75f, Qt::AlignCenter);

    // ---- content (panel2, 91,74,646x311) ----
    QWidget *panel2 = makePanel(central, 91, 74, 646, 272);
    m_pages = new QStackedWidget(panel2);
    m_pages->setGeometry(3, 3, 633, 266);
    m_sideButtons[0]->setChecked(true);

    // ================= PLAY page (tabPage1) =================
    QWidget *play = new QWidget();
    makeLabel(play, 118, 7, 79, 13, "Server Address");
    m_addressBox = makeLine(play, 8, 23, 297, 20, "localhost:53640");
    const QString cfgIp = m_userConfig->readSetting("IP");
    if (!cfgIp.isEmpty() && cfgIp != "N/A") {
        m_addressBox->setText(cfgIp + ":" + m_userConfig->readSetting("RobloxPort"));
    }
    makeButton(play, 311, 23, 293, 21, "SAVE SERVER", 8.25f, [this] {
        // LauncherFormShared.AddIPPortListing: append address+port to the saved lists
        const QString addr = m_addressBox->text();
        const int colon = addr.lastIndexOf(':');
        const QString ip = colon > 0 ? addr.left(colon) : addr;
        const QString port = colon > 0 ? addr.mid(colon + 1) : m_userConfig->readSetting("RobloxPort");
        appendLine(Paths::configDir + "/servers.txt", ip);
        appendLine(Paths::configDir + "/ports.txt", port);
        loadSavedLists();
        logLine("Saved server " + ip + ":" + port);
    });
    makeButton(play, 8, 50, 297, 20, "JOIN SERVER FROM IP", 8.25f, [this] { doLaunch(ScriptType_Client); });
    makeButton(play, 311, 50, 293, 20, "BROWSE SERVERS", 8.25f, [this] {
        ServerBrowser browser(this);
        browser.exec();
    });
    makeButton(play, 163, 76, 293, 30, "PLAY SOLO", 8.25f, [this] { doLaunch(ScriptType_Solo); });
    auto *sepLine = new QLabel(play);
    sepLine->setGeometry(6, 125, 612, 2);
    sepLine->setFrameShape(QFrame::Box);
    QLabel *note = makeLabel(play, 6, 134, 613, 112,
        "NOTE: \n- If you have a problem where you can't see your character, REGENERATE YOUR "
        "PLAYER ID THEN REJOIN THE SERVER. \n- If you tried to connect to the server and you "
        "get an error, REJOIN THE SERVER.\n- To reset your character, type !!!reset into chat.\n"
        "- Your tripcode will only be seen by you and the server host of any server you join. "
        "It is used to verify that you are a unique player.",
        8.25f, Qt::AlignCenter);
    note->setWordWrap(true);
    note->setStyleSheet("color: red; font-weight: bold;");
    m_pages->addWidget(play);

    // ================= HOST page (tabPage2) =================
    QWidget *host = new QWidget();
    makeLabel(host, 6, 5, 129, 13, "SERVER INFORMATION");
    m_serverInfo = makeEdit(host, 6, 21, 612, 103);
    m_serverInfo->setReadOnly(true);
    auto *sep2 = new QLabel(host);
    sep2->setGeometry(6, 153, 612, 2);
    sep2->setFrameShape(QFrame::Box);
    makeLabel(host, 141, 161, 65, 16, "Server Port");
    m_portBox = new QSpinBox(host);
    m_portBox->setGeometry(212, 159, 63, 20);
    m_portBox->setRange(1, 65535);
    m_portBox->setValue(m_userConfig->readSettingInt("RobloxPort"));
    makeLabel(host, 283, 161, 60, 16, "Player Limit");
    m_limitBox = new QSpinBox(host);
    m_limitBox->setGeometry(348, 159, 108, 20);
    m_limitBox->setRange(1, 99);
    m_limitBox->setValue(m_userConfig->readSettingInt("PlayerLimit"));
    m_upnpBox = new QCheckBox("UPnP", host);
    m_notifBox = new QCheckBox("Notifications", host);
    m_upnpBox->setGeometry(310, 185, 55, 17);
    m_notifBox->setGeometry(371, 185, 84, 17);
    Fit::row({m_upnpBox, m_notifBox}, 14);
    makeButton(host, 144, 183, 131, 24, "RESET PORT", 8.25f, [this] {
        m_portBox->setValue(53640);
        m_userConfig->saveSettingInt("RobloxPort", 53640);
    });
    makeLabel(host, 32, 132, 164, 13, "SERVER BROWSER OPTIONS:");
    makeLabel(host, 200, 132, 35, 13, "Name");
    m_browserName = makeLine(host, 241, 129, 84, 20,
                             m_userConfig->readSetting("ServerBrowserName"));
    makeLabel(host, 340, 124, 117, 26, "Master Server Address \n(Port Optional)");
    m_browserAddress = makeLine(host, 475, 129, 100, 20,
                                m_userConfig->readSetting("MasterServerAddress"));
    makeButton(host, 6, 213, 299, 36, "START SERVER", 8.25f, [this] { doLaunch(ScriptType_Server); });
    makeButton(host, 311, 213, 299, 36, "START SERVER WITH NO GRAPHICS", 8.25f, [this] {
        // ClientLoadOptions_NoGraphicsOptions variant; launch normally for now
        logLine("NOTE: 'no graphics' load option is not implemented; starting the normal server.");
        doLaunch(ScriptType_Server);
    });
    connect(m_portBox, &QSpinBox::valueChanged, this, [this](int v) {
        m_userConfig->saveSettingInt("RobloxPort", v);
        m_currentPort->setText(QString::number(v));
        loadServerInfo();
    });
    connect(m_limitBox, &QSpinBox::valueChanged, this, [this](int v) {
        m_userConfig->saveSettingInt("PlayerLimit", v);
        loadServerInfo();
    });
    connect(m_upnpBox, &QCheckBox::toggled, this, [this](bool on) {
        m_userConfig->saveSettingBool("UPnP", on);
    });
    connect(m_notifBox, &QCheckBox::toggled, this, [this](bool on) {
        m_userConfig->saveSettingBool("ShowServerNotifications", on);
    });
    m_pages->addWidget(host);

    // ================= VERSIONS page (tabPage3) =================
    QWidget *versions = new QWidget();
    m_clientList = new QListWidget(versions);
    m_clientList->setGeometry(6, 7, 612, 108);
    m_clientDesc = makeLabel(versions, 6, 118, 612, 58, "", 8.25f, Qt::AlignTop);
    m_clientDesc->setWordWrap(true);
    m_clientWarning = makeEdit(versions, 6, 179, 612, 80);
    m_clientWarning->setReadOnly(true);
    connect(m_clientList, &QListWidget::currentTextChanged, this, [this](const QString &c) {
        m_userConfig->saveSetting("SelectedClient", c);
        m_selectedClientLabel->setText(c);
        ClientInfo info;
        if (ClientInfoFile::load(Paths::clientDir, c, &info)) {
            m_clientDesc->setText(info.description);
            m_clientWarning->setPlainText(info.warning);
        } else {
            m_clientDesc->setText("No clientinfo.nov found for this client.");
            m_clientWarning->clear();
        }
        loadServerInfo();
        logLine("Selected client: " + c);
    });
    m_pages->addWidget(versions);

    // ================= MAPS page (tabPage4) =================
    QWidget *maps = new QWidget();
    m_searchBar = makeLine(maps, 7, 5, 253, 20);
    auto *searchBtn = new QPushButton(maps);
    searchBtn->setGeometry(269, 3, 23, 23);
    searchBtn->setIcon(QIcon(":/assets/search.png"));
    searchBtn->setFlat(true);
    auto *refreshBtn = new QPushButton(maps);
    refreshBtn->setGeometry(298, 3, 25, 23);
    refreshBtn->setIcon(QIcon(":/assets/refresh.png"));
    refreshBtn->setFlat(true);
    makeButton(maps, 435, 3, 75, 23, "Add Map...", 8.25f, [this] {
        const QString file = QFileDialog::getOpenFileName(this, "Add Map",
            Paths::mapsDir, "Roblox Maps (*.rbxl *.rbxlx *.rbxl.bz2 *.rbxlx.bz2)");
        if (file.isEmpty()) return;
        const QString dest = Paths::mapsDirCustom + "/" + QFileInfo(file).fileName();
        QDir().mkpath(Paths::mapsDirCustom);
        QFile::remove(dest);
        QFile::copy(file, dest);
        loadMaps();
        logLine("Added map " + dest);
    });
    makeButton(maps, 516, 3, 102, 23, "Open Maps Folder", 8.25f, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::mapsDir));
    });
    m_mapsTree = new QTreeWidget(maps);
    m_mapsTree->setHeaderHidden(true);
    m_mapsTree->setGeometry(7, 29, 611, 108);
    m_mapDesc = makeEdit(maps, 7, 143, 611, 110);
    m_mapDesc->setReadOnly(true);
    connect(searchBtn, &QPushButton::clicked, this, [this] { loadMaps(m_searchBar->text()); });
    connect(m_searchBar, &QLineEdit::returnPressed, this, [this] { loadMaps(m_searchBar->text()); });
    connect(refreshBtn, &QPushButton::clicked, this, [this] {
        m_searchBar->clear();
        loadMaps();
    });
    connect(m_mapsTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        if (!it || !it->parent()) return;
        const QString map = it->text(0);
        const QString path = Paths::mapsDir + "/" + it->parent()->text(0) + "/" + map;
        m_userConfig->saveSetting("Map", map);
        m_userConfig->saveSetting("MapPath", path);
        m_userConfig->saveSetting("MapPathSnip", "maps/" + it->parent()->text(0) + "/" + map);
        m_selectedMapLabel->setText(map);
        // The original shows a map description here; files don't carry metadata.
        m_mapDesc->setPlainText("Selected map: " + map + "\nPath: " + path);
        loadServerInfo();
        logLine("Selected map: " + map);
    });
    m_pages->addWidget(maps);

    // ================= SAVED page (tabPage6) =================
    QWidget *saved = new QWidget();
    makeLabel(saved, 3, 3, 295, 15, "SERVERS");
    m_serversList = new QListWidget(saved);
    m_serversList->setGeometry(6, 21, 292, 173);
    makeLabel(saved, 325, 3, 292, 15, "PORTS");
    m_portsList = new QListWidget(saved);
    m_portsList->setGeometry(325, 21, 292, 173);
    makeButton(saved, 57, 200, 60, 23, "Add", 8.25f, [this] {
        const int colon = m_addressBox->text().lastIndexOf(':');
        appendLine(Paths::configDir + "/servers.txt",
                   colon > 0 ? m_addressBox->text().left(colon) : m_addressBox->text());
        loadSavedLists();
    });
    makeButton(saved, 120, 200, 62, 23, "Remove", 8.25f, [this] {
        delete m_serversList->currentItem();
    });
    makeButton(saved, 188, 200, 55, 23, "Reset", 8.25f, [this] { loadSavedLists(); });
    makeButton(saved, 381, 201, 54, 23, "Add", 8.25f, [this] {
        appendLine(Paths::configDir + "/ports.txt", m_portBox->text());
        loadSavedLists();
    });
    makeButton(saved, 441, 201, 60, 23, "Remove", 8.25f, [this] {
        delete m_portsList->currentItem();
    });
    makeButton(saved, 507, 201, 60, 23, "Reset", 8.25f, [this] { loadSavedLists(); });
    makeLabel(saved, 92, 227, 136, 12, "CURRENT SERVER IP:");
    m_currentIp = makeLabel(saved, 92, 241, 120, 20, "localhost");
    makeLabel(saved, 396, 227, 149, 12, "CURRENT SERVER PORT:");
    m_currentPort = makeLabel(saved, 396, 241, 120, 20, m_portBox->text());
    m_pages->addWidget(saved);

    // ================= CHANGES page (tabPage8) =================
    QWidget *changes = new QWidget();
    auto *changelog = makeEdit(changes, 3, 3, 616, 256);
    changelog->setReadOnly(true);
    QFile cl(Paths::basePath + "/changelog.txt");
    if (cl.open(QIODevice::ReadOnly | QIODevice::Text)) {
        changelog->setPlainText(QString::fromUtf8(cl.readAll()));
    }
    m_pages->addWidget(changes);

    // ================= OPTIONS page (tabPage5) =================
    QWidget *options = new QWidget();
    auto *panel5 = makePanel(options, 0, 0, 625, 61);
    m_discordBox = new QCheckBox("Discord Rich Presence", panel5);
    m_discordBox->setGeometry(96, 6, 135, 17);
    m_localPlayBox = new QCheckBox("Local Play", panel5);
    m_localPlayBox->setGeometry(237, 5, 77, 19);
    m_minimizeBox = new QCheckBox("Minimize on Launch", panel5);
    m_minimizeBox->setGeometry(320, 6, 124, 17);
    m_webProxyBox = new QCheckBox("Web Proxy", panel5);
    m_webProxyBox->setGeometry(450, 6, 78, 17);
    Fit::row({m_discordBox, m_localPlayBox, m_minimizeBox, m_webProxyBox}, 14);
    makeButton(panel5, 71, 27, 83, 26, "Reset Config", 8.25f, [this] {
        if (QMessageBox::question(this, "Novetus - Reset",
                                  "Reset the launcher configuration? The launcher will restart.")
            != QMessageBox::Yes) {
            return;
        }
        FileManagement::resetConfigValues(m_userConfig, m_userConfig->readSettingInt("LauncherStyle"),
                                          m_userConfig->readSetting("SelectedClient"),
                                          m_userConfig->readSetting("Map"));
        FileManagement::resetCustomizationValues(m_customConfig);
        logLine("Configuration reset. Restarting the launcher...");
        qApp->quit();
    });
    makeButton(panel5, 160, 27, 108, 26, "Clear Asset Cache", 8.25f, [this] {
        QDir(Paths::dataPath + "/assetcache").removeRecursively();
        QDir().mkpath(Paths::dataPath + "/assetcache");
        logLine("Asset cache cleared.");
    });
    makeButton(panel5, 274, 27, 100, 26, "Graphics Settings", 8.25f, [this] {
        // button36_Click parity: opens the settings window
        SettingsDialog w(this);
        w.exec();
    });
    makeLabel(panel5, 377, 32, 78, 13, "Launcher Style");
    m_styleBox = new QComboBox(panel5);
    m_styleBox->setGeometry(459, 30, 103, 21);
    m_styleBox->addItems({"None", "Extended", "Compact", "Stylish"});
    m_styleBox->setCurrentIndex(std::clamp(m_userConfig->readSettingInt("LauncherStyle"), 0, 3));
    connect(m_styleBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_userConfig->saveSettingInt("LauncherStyle", i);
        logLine("Launcher style set to " + m_styleBox->itemText(i) + " — restart to apply.");
    });
    // Wine launch settings (WineCommand / WinePrefix in config.json)
    auto *wineCmdLabel = makeLabel(options, 8, 71, 88, 16, "Wine Command:", 8.25f);
    Q_UNUSED(wineCmdLabel);
    auto *wineCmd = new QLineEdit(options);
    wineCmd->setGeometry(98, 68, 150, 20);
    wineCmd->setFont(QFont("Microsoft Sans Serif", 8.25f));
    wineCmd->setPlaceholderText("wine");
    wineCmd->setText(m_userConfig->readSetting("WineCommand"));
    auto *winePrefixLabel = makeLabel(options, 258, 71, 74, 16, "Wine Prefix:", 8.25f);
    Q_UNUSED(winePrefixLabel);
    auto *winePrefixBox = new QLineEdit(options);
    winePrefixBox->setGeometry(334, 68, 200, 20);
    winePrefixBox->setFont(QFont("Microsoft Sans Serif", 8.25f));
    winePrefixBox->setPlaceholderText("(default)");
    winePrefixBox->setText(LinuxCompat::winePrefix());
    auto *winePrefixBrowse = new QPushButton("Browse...", options);
    winePrefixBrowse->setGeometry(540, 68, 72, 20);
    winePrefixBrowse->setFont(QFont("Microsoft Sans Serif", 8.25f));
    connect(winePrefixBrowse, &QPushButton::clicked, this, [this, winePrefixBox] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Select the Wine prefix", winePrefixBox->text());
        if (!dir.isEmpty()) winePrefixBox->setText(dir);
    });
    connect(wineCmd, &QLineEdit::editingFinished, this, [this, wineCmd] {
        m_userConfig->saveSetting("WineCommand", wineCmd->text().trimmed());
    });
    connect(winePrefixBox, &QLineEdit::editingFinished, this, [this, winePrefixBox] {
        m_userConfig->saveSetting("WinePrefix", winePrefixBox->text().trimmed());
    });

    auto *readme = makeEdit(options, 11, 92, 601, 131);
    readme->setReadOnly(true);
    QFile rm(Paths::basePath + "/README-AND-CREDITS.TXT");
    if (rm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        readme->setPlainText(QString::fromUtf8(rm.readAll()));
    }
    makeLabel(options, 11, 226, 601, 16,
              "Novetus is a Roblox multilauncher built for the community.", 8.25f);
    makeLabel(options, 8, 247, 295, 16, "EDGE Snapshot v26.0.2 (native C++ build)", 8.25f);
    makeLabel(options, 309, 247, 303, 16, "PROJECT STARLIGHT", 8.25f, Qt::AlignRight);
    m_pages->addWidget(options);

    connect(m_discordBox, &QCheckBox::toggled, this, [this](bool on) {
        m_userConfig->saveSettingBool("DiscordRichPresence", on);
    });
    connect(m_localPlayBox, &QCheckBox::toggled, this, [this](bool on) {
        m_userConfig->saveSettingBool("LocalPlay", on);
    });
    connect(m_minimizeBox, &QCheckBox::toggled, this, [this](bool on) {
        m_userConfig->saveSettingBool("CloseOnLaunch", on);
    });
    connect(m_webProxyBox, &QCheckBox::toggled, this, [this](bool on) {
        m_userConfig->saveSettingBool("WebProxyEnabled", on);
    });

    loadClients();
    loadMaps();
    loadSavedLists();
    loadServerInfo();
    syncOptionsFromConfig();
    fetchExternalIP();
}

// ---- widget factories ------------------------------------------------------

QFrame *ExtendedWindow::makePanel(QWidget *parent, int x, int y, int w, int h)
{
    auto *p = new QFrame(parent);
    p->setGeometry(x, y, w, h);
    p->setFrameShape(QFrame::StyledPanel);
    p->setAutoFillBackground(true);
    return p;
}

QPushButton *ExtendedWindow::makeButton(QWidget *parent, int x, int y, int w, int h,
                                        const QString &text, float ptSize,
                                        std::function<void()> onClick)
{
    auto *b = new QPushButton(text, parent);
    b->setGeometry(x, y, w, h);
    QFont f("Microsoft Sans Serif", ptSize);
    b->setFont(f);
    if (onClick) connect(b, &QPushButton::clicked, onClick);
    return b;
}

QLabel *ExtendedWindow::makeLabel(QWidget *parent, int x, int y, int w, int h,
                                  const QString &text, float ptSize, Qt::Alignment align)
{
    auto *l = new QLabel(text, parent);
    l->setGeometry(x, y, w, h);
    l->setAlignment(align);
    l->setFont(QFont("Microsoft Sans Serif", ptSize));
    return l;
}

QLineEdit *ExtendedWindow::makeLine(QWidget *parent, int x, int y, int w, int h, const QString &text)
{
    auto *e = new QLineEdit(parent);
    e->setGeometry(x, y, w, h);
    e->setText(text);
    e->setFont(QFont("Microsoft Sans Serif", 8.25f));
    return e;
}

QPlainTextEdit *ExtendedWindow::makeEdit(QWidget *parent, int x, int y, int w, int h)
{
    auto *e = new QPlainTextEdit(parent);
    e->setGeometry(x, y, w, h);
    e->setFont(QFont("Microsoft Sans Serif", 8.25f));
    return e;
}

// ---- behavior ---------------------------------------------------------------

void ExtendedWindow::logLine(const QString &msg)
{
    qInfo().noquote() << msg;
}

void ExtendedWindow::pickSplash()
{
    // SplashReader: date-based special splash wins, otherwise a random splash.
    QString text;
    QFile special(Paths::configDir + "/splashes-special.txt");
    if (special.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString today = QDate::currentDate().toString("M/d");
        const QStringList lines = QString::fromUtf8(special.readAll())
                                      .split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
        for (const QString &l : lines) {
            const QStringList parts = l.split('|');
            if (parts.size() >= 3 && parts[2].trimmed() == today) {
                text = parts[0];
                break;
            }
        }
    }
    if (text.isEmpty()) {
        QFile f(Paths::configDir + "/splashes.txt");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QStringList lines = QString::fromUtf8(f.readAll())
                                          .split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                text = lines[QRandomGenerator::global()->bounded(lines.size())].split('|').first();
            }
        }
    }
    // DecodeSplashString token pass
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
    QString random;
    const int len = 2 + (int)(QRandomGenerator::global()->generate() % 31);
    for (int i = 0; i < len; i++) {
        random += chars[QRandomGenerator::global()->bounded(chars.size())];
    }
    const QDateTime now = QDateTime::currentDateTime();
    text.replace("%name%", m_userConfig->readSetting("PlayerName", true))
        .replace("%randomtext%", random)
        .replace("%version%", "26.0.2")
        .replace("%year%", QString::number(now.date().year()))
        .replace("%day%", QString::number(now.date().day()))
        .replace("%month%", QString::number(now.date().month()))
        .replace("%nextyear%", QString::number(now.date().year() + 1))
        .replace("%newline%", "\n")
        .replace("%branch%", "1.4")
        .replace("[normal]", "")
        .replace("[stylish]", "");
    m_splash->setText(text);
}

void ExtendedWindow::loadClients()
{
    Launcher l(ScriptContext{});
    m_clientList->clear();
    m_clientList->addItems(l.listClients());
    const QString selected = m_userConfig->readSetting("SelectedClient");
    const auto items = m_clientList->findItems(selected, Qt::MatchExactly);
    if (!items.isEmpty()) {
        m_clientList->setCurrentItem(items.first());
    }
}

void ExtendedWindow::loadMaps(const QString &filter)
{
    m_mapsTree->clear();
    auto *top = new QTreeWidgetItem(m_mapsTree, {"maps"});
    QDir maps(Paths::mapsDir);
    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(filter),
                                QRegularExpression::CaseInsensitiveOption);
    for (const QFileInfo &fi :
         maps.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!fi.suffix().startsWith("rbxl")) continue;
        if (!filter.isEmpty() && !re.match(fi.fileName()).hasMatch()) continue;
        new QTreeWidgetItem(top, {fi.fileName()});
    }
    for (const QString &sub : maps.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QTreeWidgetItem *cat = nullptr;
        QDir subDir(Paths::mapsDir + "/" + sub);
        for (const QFileInfo &fi :
             subDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (!fi.suffix().startsWith("rbxl")) continue;
            if (!filter.isEmpty() && !re.match(fi.fileName()).hasMatch()) continue;
            if (!cat) cat = new QTreeWidgetItem(top, {sub});
            new QTreeWidgetItem(cat, {fi.fileName()});
        }
    }
    top->setExpanded(true);
}

void ExtendedWindow::loadSavedLists()
{
    m_serversList->clear();
    m_serversList->addItems(readLines(Paths::configDir + "/servers.txt"));
    m_portsList->clear();
    m_portsList->addItems(readLines(Paths::configDir + "/ports.txt"));
}

void ExtendedWindow::fetchExternalIP()
{
    if (!m_net) {
        m_net = new QNetworkAccessManager(this);
    }
    QNetworkRequest req(QUrl("https://ipv4.icanhazip.com/"));
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return; // keep localhost fallback
        }
        const QString ip = QString::fromUtf8(reply->readAll()).trimmed();
        if (!ip.isEmpty()) {
            m_externalIP = ip;
            loadServerInfo();
        }
    });
}

void ExtendedWindow::loadServerInfo()
{
    // NovetusFuncs.LoadServerInformation parity: identity + shareable join URI.
    const QString client = m_userConfig->readSetting("SelectedClient");
    const QString port = QString::number(m_userConfig->readSettingInt("RobloxPort"));
    const QString map = m_userConfig->readSetting("Map");
    const QString ip = m_externalIP;

    // novetus://<b64( b64(ip)|b64(port)|b64(client) )> — original URI format
    const QStringList uriFields = {
        Crypto::base64Encode(ip),
        Crypto::base64Encode(port),
        Crypto::base64Encode(client)
    };
    const QString uri = "novetus://" + Crypto::base64Encode(uriFields.join('|'));

    m_serverInfo->setPlainText(
        "Client: " + client
        + "\nIP: " + ip
        + "\nPort: " + port
        + "\nMap: " + map
        + "\nPlayers: " + QString::number(m_userConfig->readSettingInt("PlayerLimit"))
        + "\nVersion: Novetus " + GuiShared::productVersion()
        + "\nOnline URI Link: " + uri);
    m_currentIp->setText(m_userConfig->readSetting("IP").isEmpty()
                         ? "localhost" : m_userConfig->readSetting("IP"));
    m_currentPort->setText(m_portBox->text());
}

void ExtendedWindow::syncOptionsFromConfig()
{
    QSignalBlocker b1(m_discordBox), b2(m_localPlayBox), b3(m_minimizeBox), b4(m_webProxyBox);
    m_discordBox->setChecked(m_userConfig->readSettingBool("DiscordRichPresence"));
    m_localPlayBox->setChecked(m_userConfig->readSettingBool("LocalPlay"));
    m_minimizeBox->setChecked(m_userConfig->readSettingBool("CloseOnLaunch"));
    m_webProxyBox->setChecked(m_userConfig->readSettingBool("WebProxyEnabled"));
    m_upnpBox->setChecked(m_userConfig->readSettingBool("UPnP"));
    m_notifBox->setChecked(m_userConfig->readSettingBool("ShowServerNotifications"));
}

ScriptContext ExtendedWindow::makeContext(ScriptType type)
{
    ScriptContext ctx;
    ctx.userConfig = m_userConfig;
    ctx.customConfig = m_customConfig;
    ctx.clientName = m_userConfig->readSetting("SelectedClient");
    ctx.launcherExePath = QFileInfo("/proc/self/exe").canonicalFilePath();
    ctx.tripcode = GuiShared::computeTripcode();
    const QString addr = m_addressBox->text();
    const int colon = addr.lastIndexOf(':');
    ctx.currentServerIP = colon > 0 ? addr.left(colon) : "localhost";
    ctx.currentServerPort = colon > 0 ? addr.mid(colon + 1).toInt() : 53640;
    if (type == ScriptType_Client && m_serversList->currentItem()) {
        // joining from the SAVED list would override; kept simple
    }
    return ctx;
}

bool ExtendedWindow::doLaunch(ScriptType type, bool no3d)
{
    QString err;
    Launcher launcher(makeContext(type));
    logLine("Launching " + m_userConfig->readSetting("SelectedClient") + " ...");
    // C# StartGame parity: joining a server passes no map (the server sends
    // the place); solo launches the client with nomap as well.
    const bool nomap = (type == ScriptType_Client || type == ScriptType_Solo);
    if (!launcher.launch(type, no3d, nomap, QString(), &err)) {
        QMessageBox::critical(this, "Novetus - Error",
                              "Failed to launch Novetus. (Error: "
                              + (err.isEmpty() ? QString("unknown error") : err) + ")");
        return false;
    }
    logLine("Client launched.");
    if (m_minimizeBox->isChecked()) {
        showMinimized();
    }
    return true;
}
