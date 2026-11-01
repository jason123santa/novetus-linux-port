#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QPainter>
#include <QEvent>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTabWidget>
#include <QApplication>

#include "core/paths.h"
#include "core/config.h"

// ---------------------------------------------------------------------------
// ImageButton — paints the original PNG button art with hover/click states.
// ---------------------------------------------------------------------------
ImageButton::ImageButton(const QString &assetBase, const QString &toolTip, QWidget *parent)
    : QPushButton(parent)
{
    m_normal.load(":/assets/" + assetBase + ".png");
    m_hover.load(":/assets/" + assetBase + "_hover.png");
    m_pressed.load(":/assets/" + assetBase + "_click.png");
    setFixedSize(m_normal.size());
    setToolTip(toolTip);
    setFlat(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

void ImageButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QPixmap *img = &m_normal;
    if (m_pressedNow && !m_pressed.isNull()) {
        img = &m_pressed;
    } else if (m_hovering && !m_hover.isNull()) {
        img = &m_hover;
    }
    if (!img->isNull()) {
        p.drawPixmap(rect(), *img);
    }
}

bool ImageButton::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::HoverEnter:
    case QEvent::Enter:
        m_hovering = true;
        update();
        break;
    case QEvent::HoverLeave:
    case QEvent::Leave:
        m_hovering = false;
        update();
        break;
    case QEvent::MouseButtonPress:
        m_pressedNow = true;
        update();
        break;
    case QEvent::MouseButtonRelease:
        m_pressedNow = false;
        update();
        break;
    default:
        break;
    }
    return QPushButton::event(e);
}

// ---------------------------------------------------------------------------
// MainWindow — mirrors the Novetus "Stylish" launcher: logo banner, orange
// splash bar, image buttons, maps tree, changelog/readme tabs.
// ---------------------------------------------------------------------------
namespace {
const QColor kBackground(237, 237, 237);
const QColor kSplashOrange(245, 135, 13);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Novetus EDGE Snapshot v26.0.2 (native Linux)");
    setWindowIcon(QIcon(":/assets/playsolo.png"));
    resize(760, 620);

    m_userConfig = new Config(QStringLiteral("Config"), Paths::configDir + "/config.json");
    m_customConfig = new Config(QStringLiteral("Customization"),
                                Paths::configDir + "/config_customization.json");
    m_userConfig->load();
    m_customConfig->load();

    auto *central = new QWidget(this);
    central->setStyleSheet(QString("QWidget { background: rgb(%1,%2,%3); }")
                               .arg(kBackground.red()).arg(kBackground.green())
                               .arg(kBackground.blue()));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);

    // --- Logo banner ---
    auto *logo = new QLabel(central);
    QPixmap logoPix(":/assets/logo.png");
    logo->setPixmap(logoPix.scaled(560, logoPix.height() * 560 / logoPix.width(),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo);

    // --- Splash bar (orange, white bold text, random splash) ---
    m_splash = new QLabel(central);
    m_splash->setAlignment(Qt::AlignCenter);
    m_splash->setStyleSheet(
        QString("QLabel { background: rgb(%1,%2,%3); color: white; "
                "font-weight: bold; font-size: 14px; border: 1px solid white; "
                "padding: 6px; }")
            .arg(kSplashOrange.red()).arg(kSplashOrange.green()).arg(kSplashOrange.blue()));
    pickSplash();
    root->addWidget(m_splash);

    // --- Body: maps tree + game controls ---
    auto *body = new QHBoxLayout();
    body->setSpacing(8);

    auto *mapsGroup = new QGroupBox("MAPS", central);
    auto *mapsLayout = new QVBoxLayout(mapsGroup);
    m_mapsTree = new QTreeWidget(mapsGroup);
    m_mapsTree->setHeaderLabel("Maps");
    m_mapsTree->setStyleSheet("QTreeWidget { background: rgb(237,237,237); }");
    connect(m_mapsTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        if (it && !it->text(0).isEmpty() && it->parent()) {
            m_userConfig->saveSetting("Map", it->text(0));
            m_userConfig->saveSetting("MapPath", Paths::mapsDir + "/"
                                                  + it->parent()->text(0) + "/" + it->text(0));
            m_userConfig->saveSetting("MapPathSnip", "maps/" + it->parent()->text(0)
                                                          + "/" + it->text(0));
            m_selectedMap->setText(it->text(0));
            logLine("Selected map: " + it->text(0));
        }
    });
    mapsLayout->addWidget(m_mapsTree);
    auto *refreshBtn = new QPushButton(mapsGroup);
    refreshBtn->setIcon(QIcon(":/assets/refresh.png"));
    refreshBtn->setFlat(true);
    refreshBtn->setFixedSize(20, 20);
    refreshBtn->setToolTip("Refresh maps");
    connect(refreshBtn, &QPushButton::clicked, this, [this] { refreshClients(); });
    mapsLayout->addWidget(refreshBtn, 0, Qt::AlignRight);
    body->addWidget(mapsGroup, 3);

    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(6);

    auto *gameGroup = new QGroupBox("GAME", central);
    auto *gameLayout = new QVBoxLayout(gameGroup);
    m_selectedClient = new QLabel("SELECTED CLIENT:", gameGroup);
    m_clientBox = new QComboBox(gameGroup);
    m_selectedMap = new QLabel("SELECTED MAP:", gameGroup);
    gameLayout->addWidget(m_selectedClient);
    gameLayout->addWidget(m_clientBox);
    gameLayout->addWidget(m_selectedMap);

    auto *btnRow1 = new QHBoxLayout();
    auto *playSoloBtn = new ImageButton("playsolo", "Play Solo", gameGroup);
    auto *joinBtn = new ImageButton("joinserver", "Join a server", gameGroup);
    btnRow1->addWidget(playSoloBtn);
    btnRow1->addWidget(joinBtn);
    gameLayout->addLayout(btnRow1);
    auto *serverBtn = new ImageButton("startserver", "Start a server", gameGroup);
    gameLayout->addWidget(serverBtn, 0, Qt::AlignLeft);

    connect(playSoloBtn, &QPushButton::clicked, this, [this] { launchGame(ScriptType_Solo); });
    connect(joinBtn, &QPushButton::clicked, this, [this] { launchGame(ScriptType_Client); });
    connect(serverBtn, &QPushButton::clicked, this, [this] { launchGame(ScriptType_Server); });
    rightCol->addWidget(gameGroup);

    auto *serverGroup = new QGroupBox("SERVER INFORMATION", central);
    auto *serverLayout = new QVBoxLayout(serverGroup);
    m_serverInfo = new QPlainTextEdit(serverGroup);
    m_serverInfo->setReadOnly(true);
    m_serverInfo->setMaximumHeight(90);
    m_serverInfo->setStyleSheet("QPlainTextEdit { background: rgb(237,237,237); }");
    serverLayout->addWidget(m_serverInfo);
    rightCol->addWidget(serverGroup);
    rightCol->addStretch(1);
    body->addLayout(rightCol, 2);

    root->addLayout(body, 1);

    // --- Bottom tabs: CHANGES / README / CONSOLE ---
    m_tabs = new QTabWidget(central);
    auto *changelog = new QPlainTextEdit(m_tabs);
    changelog->setReadOnly(true);
    QFile changes(Paths::basePath + "/changelog.txt");
    if (changes.open(QIODevice::ReadOnly | QIODevice::Text)) {
        changelog->setPlainText(QString::fromUtf8(changes.readAll()));
    }
    auto *readme = new QPlainTextEdit(m_tabs);
    readme->setReadOnly(true);
    QFile rm(Paths::basePath + "/README-AND-CREDITS.TXT");
    if (rm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        readme->setPlainText(QString::fromUtf8(rm.readAll()));
    }
    m_console = new QPlainTextEdit(m_tabs);
    m_console->setReadOnly(true);
    m_tabs->addTab(changelog, "CHANGES");
    m_tabs->addTab(readme, "README");
    m_tabs->addTab(m_console, "CONSOLE");
    root->addWidget(m_tabs, 1);

    setCentralWidget(central);

    connect(m_clientBox, &QComboBox::currentTextChanged, this, [this](const QString &c) {
        m_userConfig->saveSetting("SelectedClient", c);
        m_selectedClient->setText("SELECTED CLIENT: " + c);
        logLine("Selected client: " + c);
    });

    refreshClients();
    m_serverInfo->setPlainText("Server IP: " + (m_userConfig->readSetting("IP").isEmpty()
        ? "localhost" : m_userConfig->readSetting("IP"))
        + "\nServer Port: " + m_userConfig->readSetting("RobloxPort"));
}

void MainWindow::pickSplash()
{
    QFile f(Paths::configDir + "/splashes.txt");
    QString text = "Novetus!";
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(f.readAll())
                                      .split(QRegularExpression("\\r\\n|\\n"),
                                             Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            QString pick = lines[QRandomGenerator::global()->bounded(lines.size())];
            pick = pick.split('|').first();
            text = pick;
        }
    }

    // Splash.DecodeSplashString — token substitution
    const QString playerName = m_userConfig->readSetting("PlayerName", true);
    QString random;
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
    const int len = 2 + (int)(QRandomGenerator::global()->generate() % 31);
    for (int i = 0; i < len; i++) {
        random += chars[QRandomGenerator::global()->bounded(chars.size())];
    }
    const QDateTime now = QDateTime::currentDateTime();
    text.replace("%name%", playerName)
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

void MainWindow::logLine(const QString &msg)
{
    if (m_console) {
        m_console->appendPlainText("[" + QTime::currentTime().toString("HH:mm:ss") + "] " + msg);
    }
    qInfo().noquote() << msg;
}

void MainWindow::refreshClients()
{
    Launcher l(ScriptContext{});
    const QStringList clients = l.listClients();
    const QString selected = m_userConfig->readSetting("SelectedClient");
    QSignalBlocker blocker(m_clientBox);
    m_clientBox->clear();
    m_clientBox->addItems(clients);
    int idx = m_clientBox->findText(selected);
    if (idx < 0 && m_clientBox->count() > 0) idx = 0;
    if (idx >= 0) m_clientBox->setCurrentIndex(idx);
    m_selectedClient->setText("SELECTED CLIENT: " + m_clientBox->currentText());

    // Maps tree: categories by subfolder + top-level files
    m_mapsTree->clear();
    QDir maps(Paths::mapsDir);
    QTreeWidgetItem *top = new QTreeWidgetItem(m_mapsTree, {"maps"});
    for (const QFileInfo &fi :
         maps.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (fi.suffix().startsWith("rbxl")) {
            new QTreeWidgetItem(top, {fi.fileName()});
        }
    }
    for (const QString &sub :
         maps.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QTreeWidgetItem *cat = new QTreeWidgetItem(top, {sub});
        QDir subDir(Paths::mapsDir + "/" + sub);
        for (const QFileInfo &fi :
             subDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (fi.suffix().startsWith("rbxl")) {
                new QTreeWidgetItem(cat, {fi.fileName()});
            }
        }
    }
    top->setExpanded(true);

    const QString map = m_userConfig->readSetting("Map");
    if (!map.isEmpty()) {
        m_selectedMap->setText("SELECTED MAP: " + map);
    }
}

ScriptContext MainWindow::makeContext(ScriptType type)
{
    ScriptContext ctx;
    ctx.userConfig = m_userConfig;
    ctx.customConfig = m_customConfig;
    ctx.clientName = m_clientBox->currentText();
    ctx.launcherExePath = QFileInfo("/proc/self/exe").canonicalFilePath();
    ctx.currentServerIP = m_userConfig->readSetting("IP");
    if (ctx.currentServerIP.isEmpty() || ctx.currentServerIP == "N/A") {
        ctx.currentServerIP = "localhost";
    }
    return ctx;
}

void MainWindow::launchGame(ScriptType type)
{
    if (m_clientBox->currentText().isEmpty()) {
        logLine("Select a client first.");
        return;
    }
    QString err;
    Launcher launcher(makeContext(type));
    logLine("Launching " + m_clientBox->currentText() + " ...");
    if (!launcher.launch(type, false, false, QString(), &err)) {
        logLine("ERROR - launch failed: " + (err.isEmpty() ? QString("unknown") : err));
        return;
    }
    logLine("Client launched.");
}
