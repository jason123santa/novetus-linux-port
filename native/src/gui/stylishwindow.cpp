#include "stylishwindow.h"
#include "imagebutton.h"

#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QFont>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QComboBox>
#include <QRegularExpression>
#include <QTabWidget>
#include <algorithm>

#include <QDebug>
#include "serverbrowser.h"
#include "settingsdialog.h"
#include "sdkwindow.h"
#include "core/modmanager.h"
#include "core/filemanagement.h"
#include "core/paths.h"
#include "core/launcher.h"
#include "fit.h"
#include "customizationwindow.h"

namespace {
// WPF colors from the decompiled XAML
const char *kBackground = "#EDEDED";
const char *kGroupFill = "#DEDCDC";
const char *kLabelFill = "#AAA8A8";
const char *kTabSelected = "#AAC4E2";
const char *kTabUnselected = "#6E98C8";

QLabel *sgLbl(QWidget *p, int x, int y, int w, int h, const QString &t, int px = 12,
            Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *l = new QLabel(t, p);
    l->setGeometry(x, y, w, h);
    l->setAlignment(a);
    QFont f("Comic Sans MS");
    f.setPixelSize(px); // WPF FontSize is in pixels
    l->setFont(f);
    return l;
}

QLineEdit *sgLine(QWidget *p, int x, int y, int w, int h, const QString &t = {})
{
    auto *e = new QLineEdit(p);
    e->setGeometry(x, y, w, h);
    e->setText(t);
    return e;
}

QPlainTextEdit *sgEdit(QWidget *p, int x, int y, int w, int h, bool comic = false)
{
    auto *e = new QPlainTextEdit(p);
    e->setGeometry(x, y, w, h);
    if (comic) e->setFont(QFont("Comic Sans MS", 12));
    return e;
}

QFrame *sgRect(QWidget *p, int x, int y, int w, int h, const char *fill)
{
    auto *f = new QFrame(p);
    f->setGeometry(x, y, w, h);
    f->setStyleSheet(QString("background: %1; border: 1px solid black;").arg(fill));
    return f;
}

QPushButton *sgImpBtn(QWidget *p, int x, int y, int w, int h, const QString &text,
                    std::function<void()> fn, int px = 18)
{
    auto *b = new QPushButton(text, p);
    b->setGeometry(x, y, w, h);
    b->setStyleSheet(QString(
        "QPushButton { background: #F4CC35; color: black; border: 1px solid black;"
        " border-radius: 4px; font-family: 'Comic Sans MS'; font-size: %1px; }"
        "QPushButton:pressed { background: #E0BA30; }").arg(px));
    if (fn) QObject::connect(b, &QPushButton::clicked, fn);
    return b;
}
} // namespace

StylishWindow::StylishWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Novetus EDGE Snapshot v26.0.2 (native Linux)");
    setFixedSize(707, 419);
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    m_ui = GuiShared::createConfigs();

    QWidget *central = new QWidget(this);
    central->setFixedSize(707, 419);
    central->setStyleSheet(QString("QWidget { background: %1; }").arg(kBackground));
    setCentralWidget(central);

    // ---- WinForms shell: logo (12,3,683x75) + splash (9,81,689x32) ----
    QLabel *logo = new QLabel(central);
    QPixmap lp(":/assets/logo.png");
    logo->setPixmap(lp.scaled(683, 75, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setGeometry(12, 3, 683, 75);
    logo->setAlignment(Qt::AlignCenter);

    const QColor splashColors[3] = {QColor(245,135,13), QColor(255,3,2), QColor(238,154,181)};
    QLabel *splash = new QLabel(central);
    splash->setGeometry(9, 81, 689, 32);
    splash->setText(GuiShared::pickSplash(m_ui.user));
    splash->setAlignment(Qt::AlignCenter);
    QFont sf("Comic Sans MS");
    sf.setPixelSize(16); // WinForms 12pt bold ≈ 16px
    sf.setBold(true);
    splash->setFont(sf);
    const QColor &sc = splashColors[QRandomGenerator::global()->bounded(3)];
    splash->setStyleSheet(QString(
        "QLabel { background: rgb(%1,%2,%3); color: white; border: 1px solid white; }")
        .arg(sc.red()).arg(sc.green()).arg(sc.blue()));

    // ---- tab area (0,116,706x303); TabControl 689x295 centered ----
    auto *tabHost = new QWidget(central);
    tabHost->setGeometry(0, 116, 706, 303);
    tabHost->setStyleSheet(QString("QWidget { background: %1; }").arg(kBackground));

    // Tab strip: buttons styled like the WPF TabItem template
    auto *tabBar = new QWidget(tabHost);
    tabBar->setGeometry(9, 0, 689, 33);
    auto *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    auto *pages = new QStackedWidget(tabHost);
    pages->setGeometry(9, 33, 689, 295 - 33);

    const QStringList tabNames = {"PLAY", "HOST", "VERSIONS", "CHANGES", "OPTIONS"};
    QList<QPushButton *> tabButtons;
    for (int i = 0; i < tabNames.size(); i++) {
        auto *b = new QPushButton(tabNames[i], tabBar);
        b->setCheckable(true);
        b->setFixedHeight(33);
        b->setMinimumWidth(int(tabNames[i].size() * 13.0) + 24);
        QFont f("Comic Sans MS");
        f.setPixelSize(18);
        b->setFont(f);
        b->setStyleSheet(QString(
            "QPushButton { color: white; background: %1; border: 1px solid black; }"
            "QPushButton:checked { background: %2; }").arg(kTabUnselected, kTabSelected));
        tabLayout->addWidget(b);
        tabButtons << b;
        connect(b, &QPushButton::clicked, this, [this, i, tabButtons, pages] {
            for (int j = 0; j < tabButtons.size(); j++) {
                tabButtons[j]->setChecked(j == i);
            }
            pages->setCurrentIndex(i);
        });
    }
    tabButtons[0]->setChecked(true);
    tabButtons[0]->setAutoExclusive(true);

    auto page = [&pages]() {
        auto *w = new QWidget();
        w->setStyleSheet(QString("QWidget { background: %1; }").arg(kBackground));
        pages->addWidget(w);
        return w;
    };
    auto switchClient = [this](const QString &c) {
        m_ui.user->saveSetting("SelectedClient", c);
    };

    // ================= PLAY =================
    {
        QWidget *w = page();
        // user info group
        sgRect(w, 0, 47, 169, 83, kGroupFill);
        sgLbl(w, 10, 47, 50, 29, "Name:", 12, Qt::AlignCenter);
        m_userName = sgLine(w, 60, 53, 95, 20, m_ui.user->readSetting("PlayerName", true));
        connect(m_userName, &QLineEdit::textChanged, this, [this](const QString &t) {
            m_ui.user->saveSetting("PlayerName", t, true);
        });
        sgLbl(w, 20, 75, 40, 29, "ID:", 12, Qt::AlignCenter);
        m_userId = sgLine(w, 60, 81, 95, 20, QString::number(m_ui.user->readSettingInt("UserID")));
        connect(m_userId, &QLineEdit::textChanged, this, [this](const QString &t) {
            m_ui.user->saveSettingInt("UserID", t.toInt());
        });
        auto *regen = new QPushButton("Regenerate ID", w);
        regen->setGeometry(19, 106, 136, 20);
        connect(regen, &QPushButton::clicked, this, [this] {
            const int id = QRandomGenerator::global()->bounded(99999999);
            m_ui.user->saveSettingInt("UserID", id);
            m_userId->setText(QString::number(id));
        });
        // customize
        sgImpBtn(w, 0, 10, 169, 32, "Customize Player", [this] {
            CustomizationWindow cw(this);
            cw.exec();
        }, 12);
        // join server group
        sgRect(w, 0, 135, 169, 82, kGroupFill);
        sgRect(w, 0, 135, 169, 30, kLabelFill);
        sgLbl(w, 35, 134, 102, 30, "Join Server", 15);
        sgLbl(w, 20, 163, 135, 29, "Server Address:", 12, Qt::AlignCenter);
        m_address = sgLine(w, 7, 185, 156, 20, "localhost:53640");
        const QString ip = m_ui.user->readSetting("IP");
        if (!ip.isEmpty() && ip != "N/A") {
            m_address->setText(ip + ":" + m_ui.user->readSetting("RobloxPort"));
        }
        // maps group
        sgRect(w, 174, 10, 509, 207, kGroupFill);
        sgRect(w, 174, 10, 509, 30, kLabelFill);
        sgLbl(w, 402, 9, 52, 30, "Places", 15);
        m_search = sgLine(w, 183, 43, 214, 20);
        auto *searchBtn = new QPushButton(w);
        searchBtn->setGeometry(402, 46, 24, 17);
        searchBtn->setIcon(QIcon(":/assets/search.png"));
        searchBtn->setFlat(true);
        auto *refreshBtn = new QPushButton(w);
        refreshBtn->setGeometry(426, 46, 26, 17);
        refreshBtn->setIcon(QIcon(":/assets/refresh.png"));
        refreshBtn->setFlat(true);
        sgImpBtn(w, 457, 47, 72, 24, "Add Place", [this] {
            const QString file = QFileDialog::getOpenFileName(this, "Add Place", Paths::mapsDir,
                "Roblox Maps (*.rbxl *.rbxlx *.rbxl.bz2 *.rbxlx.bz2)");
            if (file.isEmpty()) return;
            QDir().mkpath(Paths::mapsDirCustom);
            QFile::copy(file, Paths::mapsDirCustom + "/" + QFileInfo(file).fileName());
        }, 12);
        m_mapsTree = new QTreeWidget(w);
        m_mapsTree->setHeaderHidden(true);
        m_mapsTree->setGeometry(183, 64, 346, 183);
        m_mapsTree->setFont(QFont("Comic Sans MS", 9));
        m_mapsDesc = sgEdit(w, 534, 43, 139, 168, true);
        m_mapsDesc->setReadOnly(true);
        auto reloadMaps = [this]() {
            m_mapsTree->clear();
            auto *root = new QTreeWidgetItem(m_mapsTree, {"maps"});
            QDir mapsDir(Paths::mapsDir);
            const QString f = m_search->text();
            const QRegularExpression re(
                f.isEmpty() ? QString() : QRegularExpression::wildcardToRegularExpression(f),
                QRegularExpression::CaseInsensitiveOption);
            for (const QFileInfo &fi : mapsDir.entryInfoList(QDir::Files, QDir::Name)) {
                if (!fi.suffix().startsWith("rbxl")) continue;
                if (!f.isEmpty() && !re.match(fi.fileName()).hasMatch()) continue;
                new QTreeWidgetItem(root, {fi.fileName()});
            }
            for (const QString &sub : mapsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                QTreeWidgetItem *cat = nullptr;
                for (const QFileInfo &fi : QDir(Paths::mapsDir + "/" + sub)
                         .entryInfoList(QDir::Files, QDir::Name)) {
                    if (!fi.suffix().startsWith("rbxl")) continue;
                    if (!f.isEmpty() && !re.match(fi.fileName()).hasMatch()) continue;
                    if (!cat) cat = new QTreeWidgetItem(root, {sub});
                    new QTreeWidgetItem(cat, {fi.fileName()});
                }
            }
            root->setExpanded(true);
        };
        reloadMaps();
        connect(searchBtn, &QPushButton::clicked, reloadMaps);
        connect(refreshBtn, &QPushButton::clicked, reloadMaps);
        connect(m_search, &QLineEdit::returnPressed, reloadMaps);
        connect(m_mapsTree, &QTreeWidget::itemDoubleClicked, this,
                [this, reloadMaps](QTreeWidgetItem *it, int) {
            if (!it || !it->parent()) return;
            const QString map = it->text(0);
            const QString path = Paths::mapsDir + "/" + it->parent()->text(0) + "/" + map;
            m_ui.user->saveSetting("Map", map);
            m_ui.user->saveSetting("MapPath", path);
            m_ui.user->saveSetting("MapPathSnip", "maps/" + it->parent()->text(0) + "/" + map);
            m_mapsDesc->setPlainText("Selected map: " + map);
            reloadMaps();
        });
        // launch buttons row
        auto *joinBtn = new ImageButton("joinserver", "Join a server", w);
        joinBtn->setParent(w);
        joinBtn->setGeometry(40, 222, 92, 33);
        connect(joinBtn, &QPushButton::clicked, this, [this] {
            GuiShared::launchGame(m_ui, ScriptType_Client, false, this);
        });
        auto *soloBtn = new ImageButton("playsolo", "Play Solo", w);
        soloBtn->setGeometry(198, 223, 92, 32);
        connect(soloBtn, &QPushButton::clicked, this, [this] {
            GuiShared::launchGame(m_ui, ScriptType_Solo, false, this);
        });
        auto *studioBtn = new ImageButton("buildsolo2", "Studio", w);
        studioBtn->setGeometry(295, 223, 92, 32);
        connect(studioBtn, &QPushButton::clicked, this, [this] {
            GuiShared::launchGame(m_ui, ScriptType_Studio, false, this);
        });
        auto *serverBtn = new ImageButton("startserver", "Start a server", w);
        serverBtn->setGeometry(392, 223, 92, 32);
        connect(serverBtn, &QPushButton::clicked, this, [this] {
            GuiShared::launchGame(m_ui, ScriptType_Server, false, this);
        });
        sgImpBtn(w, 489, 224, 171, 30, "Browse Servers", [this] {
            ServerBrowser browser(this);
browser.exec();
        }, 14);
    }

    // ================= HOST =================
    {
        QWidget *w = page();
        sgRect(w, 0, 11, 169, 203, kGroupFill);
        sgRect(w, 0, 10, 169, 30, kLabelFill);
        sgLbl(w, 28, 9, 113, 30, "Host Settings", 15);
        sgLbl(w, 3, 40, 68, 20, "Host Port:", 12);
        m_portBox = sgLine(w, 84, 45, 76, 20, m_ui.user->readSetting("RobloxPort"));
        connect(m_portBox, &QLineEdit::textChanged, this, [this](const QString &t) {
            m_ui.user->saveSetting("RobloxPort", t);
        });
        sgLbl(w, 1, 63, 86, 20, "Max Players:", 12);
        m_maxPlayers = sgLine(w, 84, 68, 76, 20, m_ui.user->readSetting("PlayerLimit"));
        connect(m_maxPlayers, &QLineEdit::textChanged, this, [this](const QString &t) {
            m_ui.user->saveSetting("PlayerLimit", t);
        });
        m_upnp = new QCheckBox("uPnP", w);
        m_upnp->setGeometry(10, 93, 80, 20);
        m_upnp->setChecked(m_ui.user->readSettingBool("UPnP"));
        connect(m_upnp, &QCheckBox::toggled, this, [this](bool on) {
            m_ui.user->saveSettingBool("UPnP", on);
        });
        m_notif = new QCheckBox("Join Alerts", w);
        m_notif->setGeometry(10, 114, 100, 20);
        m_notif->setChecked(m_ui.user->readSettingBool("ShowServerNotifications"));
        connect(m_notif, &QCheckBox::toggled, this, [this](bool on) {
            m_ui.user->saveSettingBool("ShowServerNotifications", on);
        });
        sgLbl(w, 14, 126, 140, 20, "Browser Server Name:", 12);
        sgLine(w, 16, 146, 134, 20, m_ui.user->readSetting("ServerBrowserName"));
        sgLbl(w, 11, 163, 150, 20, "Browser Master Server:", 12);
        sgLine(w, 16, 184, 135, 20, m_ui.user->readSetting("MasterServerAddress"));
        sgImpBtn(w, 0, 219, 169, 36, "Start Server", [this] {
            GuiShared::launchGame(m_ui, ScriptType_Server, false, this);
        }, 14);
        sgRect(w, 174, 11, 509, 244, kGroupFill);
        sgRect(w, 174, 10, 509, 30, kLabelFill);
        sgLbl(w, 371, 9, 114, 30, "Server Info", 15);
        m_serverInfo = sgEdit(w, 183, 43, 490, 205);
        m_serverInfo->setReadOnly(true);
        m_serverInfo->setPlainText("Selected Client: " + m_ui.user->readSetting("SelectedClient")
            + "\nServer Port: " + m_ui.user->readSetting("RobloxPort")
            + "\nPlayer Limit: " + m_ui.user->readSetting("PlayerLimit"));
    }

    // ================= VERSIONS =================
    {
        QWidget *w = page();
        sgRect(w, 0, 10, 683, 245, kGroupFill);
        m_clientList = new QListWidget(w);
        m_clientList->setGeometry(10, 19, 327, 228);
        m_clientWarning = sgEdit(w, 304, 19, 331, 103);
        m_clientWarning->setReadOnly(true);
        m_clientWarning->setStyleSheet("color: red; font-weight: bold; font-size: 10px;");
        m_clientDesc = sgEdit(w, 304, 127, 331, 120, true);
        m_clientDesc->setReadOnly(true);
        Launcher l(ScriptContext{});
        m_clientList->addItems(l.listClients());
        const auto items = m_clientList->findItems(m_ui.user->readSetting("SelectedClient"),
                                                   Qt::MatchExactly);
        if (!items.isEmpty()) m_clientList->setCurrentItem(items.first());
        connect(m_clientList, &QListWidget::currentTextChanged, this,
                [this, switchClient](const QString &c) {
            switchClient(c);
            ClientInfo info;
            if (ClientInfoFile::load(Paths::clientDir, c, &info)) {
                m_clientDesc->setPlainText(info.description);
                m_clientWarning->setPlainText(info.warning);
            }
        });
    }

    // ================= CHANGES =================
    {
        QWidget *w = page();
        sgRect(w, 0, 10, 683, 245, kGroupFill);
        auto *box = sgEdit(w, 10, 19, 663, 228, true);
        box->setReadOnly(true);
        QFile cl(Paths::basePath + "/changelog.txt");
        if (cl.open(QIODevice::ReadOnly | QIODevice::Text)) {
            box->setPlainText(QString::fromUtf8(cl.readAll()));
        }
    }

    // ================= OPTIONS =================
    {
        QWidget *w = page();
        sgRect(w, 0, 11, 683, 81, kGroupFill);
        sgRect(w, 0, 10, 683, 30, kLabelFill);
        sgLbl(w, 309, 9, 65, 30, "Options", 15);
        m_discord = new QCheckBox("Discord RPC", w);
        m_discord->setGeometry(10, 47, 100, 20);
        m_discord->setChecked(m_ui.user->readSettingBool("DiscordRichPresence"));
        connect(m_discord, &QCheckBox::toggled, this, [this](bool on) {
            m_ui.user->saveSettingBool("DiscordRichPresence", on);
        });
        m_minimize = new QCheckBox("Minimize on Launch", w);
        m_minimize->setGeometry(106, 47, 144, 20);
        m_minimize->setChecked(m_ui.user->readSettingBool("CloseOnLaunch"));
        connect(m_minimize, &QCheckBox::toggled, this, [this](bool on) {
            m_ui.user->saveSettingBool("CloseOnLaunch", on);
        });
        QPushButton *resetCfg = new QPushButton("Reset Config", w);
        resetCfg->setGeometry(273, 46, 75, 37);
        connect(resetCfg, &QPushButton::clicked, this, [this] {
            if (QMessageBox::question(this, "Novetus - Reset",
                                      "Reset the launcher configuration? The launcher will restart.")
                == QMessageBox::Yes) {
                FileManagement::resetConfigValues(m_ui.user, m_ui.user->readSettingInt("LauncherStyle"),
                                                  m_ui.user->readSetting("SelectedClient"),
                                                  m_ui.user->readSetting("Map"));
                FileManagement::resetCustomizationValues(m_ui.custom);
                qApp->quit();
            }
        });
        QPushButton *clearCache = new QPushButton("Clear Cache", w);
        clearCache->setGeometry(353, 46, 81, 16);
        connect(clearCache, &QPushButton::clicked, this, [this] {
            QDir(Paths::dataPath + "/assetcache").removeRecursively();
            QDir().mkpath(Paths::dataPath + "/assetcache");
        });
        QPushButton *sdkBtn = new QPushButton("Novetus SDK", w);
        sdkBtn->setGeometry(353, 67, 81, 24);
        connect(sdkBtn, &QPushButton::clicked, this, [this] {
            SdkWindow w(this);
            w.exec();
        });
        sgImpBtn(w, 439, 46, 119, 17, "Graphics Settings", [this] {
            // LoadSettings parity: opens the settings window
            SettingsDialog dlg(this);
            dlg.exec();
        }, 11);
        QPushButton *modBtn = new QPushButton("Install Mod Package", w);
        modBtn->setGeometry(439, 67, 119, 24);
        connect(modBtn, &QPushButton::clicked, this, [this] {
        const QString modFile = QFileDialog::getOpenFileName(
            this, "Open a mod package or addon script", QDir::homePath(),
            "Novetus Mods (*.zip *.novmod *.lua);;Mod Package (*.zip *.novmod);;Addon Script (*.lua)");
        if (modFile.isEmpty()) return;
        ModManager::InstallResult result;
        if (modFile.endsWith(".lua", Qt::CaseInsensitive)) {
            result = ModManager::installAddonScript(modFile);
        } else {
            result = ModManager::installModPackage(modFile);
        }
        if (!result.outcome.isEmpty()) {
            QMessageBox::information(this, result.ok ? "Novetus - Mod Installed" : "Novetus - Mod Error",
                                     result.outcome);
        }
    });
        QLabel *styleLabel = sgLbl(w, 568, 37, 107, 26, "Launcher Style", 12);
        Q_UNUSED(styleLabel);
        QComboBox *styleBox = new QComboBox(w);
        styleBox->setGeometry(568, 59, 107, 24);
        styleBox->addItems({"None", "Extended", "Compact", "Stylish"});
        styleBox->setCurrentIndex(std::clamp(m_ui.user->readSettingInt("LauncherStyle"), 0, 3));
        connect(styleBox, &QComboBox::currentIndexChanged, this, [this, styleBox](int i) {
            m_ui.user->saveSettingInt("LauncherStyle", i);
        });
        auto *vsep = new QFrame(w);
        vsep->setGeometry(254, 44, 5, 44);
        vsep->setFrameShape(QFrame::VLine);
        m_webProxy = new QCheckBox("Web Proxy", w);
        m_webProxy->setGeometry(85, 67, 100, 20);
        m_webProxy->setChecked(m_ui.user->readSettingBool("WebProxyEnabled"));
        connect(m_webProxy, &QCheckBox::toggled, this, [this](bool on) {
            m_ui.user->saveSettingBool("WebProxyEnabled", on);
        });
        Fit::row({m_discord, m_minimize, m_webProxy}, 14);
        // readme area
        auto *readme = sgEdit(w, 0, 97, 683, 129, true);
        readme->setReadOnly(true);
        QFile rm(Paths::basePath + "/README-AND-CREDITS.TXT");
        if (rm.open(QIODevice::ReadOnly | QIODevice::Text)) {
            readme->setPlainText(QString::fromUtf8(rm.readAll()));
        }
        sgLbl(w, 6, 229, 194, 29, "v" + GuiShared::productVersion(), 12, Qt::AlignCenter);
        sgLbl(w, 200, 220, 483, 20, "Tripcode: " + GuiShared::computeTripcode(), 9);
        sgLbl(w, 343, 235, 193, 24, "PROJECT STARLIGHT", 12);
    }
    // (debug removed)
    pages->setCurrentIndex(0);
    
}
