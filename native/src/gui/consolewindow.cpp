#include "consolewindow.h"

#include <QPushButton>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QCloseEvent>

#include "core/paths.h"
#include "core/webproxy.h"
#include "core/extensions.h"
#include "core/filemanagement.h"
#include "core/linuxcompat.h"
#include "sdkwindow.h"

#include <QRandomGenerator>
#include <QProcess>
#include <QMessageBox>
#include <QSysInfo>

namespace {
// ASCII banner from the decompiled NovetusConsole
const char *kBanner = R"(            NhhhhhhhhhhN                            hsssoosssd
            NyyyyhhhhyyhdN                          hoo++++++h
            NyyyyyhhhhyyyhN                         hoo+++///h
            NssssyyyyyhyyyhdN                       hsoo++///h
            Nsossssyyyyyyyyyhm                      hsoo++///h
            NsssssssssyyyyyyyhdN                    hssoo++//h
            Nsssssssssssyyyyhhhdm                   hsssoo+++h
            NyssssssssysyyyyhhhhhdN NNNNNmmmmdddddhhysoooo+++ymNN)";
} // namespace

ConsoleWindow::ConsoleWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Novetus");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    m_ui = GuiShared::createConfigs();

    QWidget *central = new QWidget(this);
    central->setFixedSize(869, 417);
    // Form BackColor = SystemColors.ControlText
    QPalette pal = central->palette();
    central->setPalette(pal);
    central->setStyleSheet("QWidget { background: palette(window-text); }");
    setCentralWidget(central);

    m_console = new QPlainTextEdit(central);
    m_console->setGeometry(0, 0, 868, 389);
    m_console->setFont(QFont("Consolas", 11));
    m_console->setReadOnly(true);
    m_console->setStyleSheet("QPlainTextEdit { background: palette(window); color: palette(window-text); border: none; }");

    m_command = new QLineEdit(central);
    m_command->setGeometry(3, 391, 783, 20);
    m_command->setFont(QFont("Microsoft Sans Serif", 8.25f));
    connect(m_command, &QLineEdit::returnPressed, this, [this] {
        processCommands(m_command->text());
    });

    auto *enter = new QPushButton("Enter", central);
    enter->setGeometry(789, 390, 75, 21);
    enter->setFont(QFont("Microsoft Sans Serif", 8.25f));
    connect(enter, &QPushButton::clicked, this, [this] {
        if (m_helpMode) {
            m_appClose = true;
            close();
        } else {
            processCommands(m_command->text());
        }
    });

    printBanner();
}

void ConsoleWindow::closeEvent(QCloseEvent *e)
{
    if (m_helpMode && !m_appClose) {
        // helpMode closes the whole app in the original; keep it simple
    }
    e->accept();
}

void ConsoleWindow::consolePrint(const QString &text, int type)
{
    QColor color = palette().color(QPalette::WindowText);
    switch (type) {
    case 0: color = Qt::black; break;
    case 2: color = Qt::red; break;
    case 3: color = Qt::green; break;
    case 4: color = Qt::cyan; break;
    case 5: color = Qt::yellow; break;
    default: break; // 1 = white (window text)
    }
    QTextCharFormat fmt;
    fmt.setForeground(color);
    QTextCursor cur = m_console->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(text + "\n", fmt);
    m_console->setTextCursor(cur);
    m_console->ensureCursorVisible();
}

void ConsoleWindow::printBanner()
{
    consolePrint(kBanner, 1);
    consolePrint("            " + QString("    Player: ")
                 + m_ui.user->readSetting("PlayerName", true) + "@novetus", 1);
    consolePrint("        Novetus version: " + GuiShared::productVersion()
                 + " (native C++ build)", 1);
    consolePrint("        Novetus path: " + Paths::basePath, 1);
    consolePrint("        Distribution Codename: " + GuiShared::branchName(), 1);
    consolePrint("        Selected Client: " + m_ui.user->readSetting("SelectedClient"), 1);
    consolePrint("        Type 'help' to list commands.", 5);
}

void ConsoleWindow::processCommands(const QString &cmd)
{
    if (cmd.isEmpty()) return;
    m_command->clear();
    consolePrint("> " + cmd, 1);

    const QString lower = cmd.toLower();
    const QStringList args = cmd.split(' ', Qt::SkipEmptyParts);

    if (lower == "help") {
        consolePrint("Commands:", 5);
        consolePrint("  client            - join a server", 1);
        consolePrint("  solo              - play solo", 1);
        consolePrint("  outfitview        - avatar 3D preview", 1);
        consolePrint("  server [3d|no3d]  - start a server", 1);
        consolePrint("  studio [map|nomap]- launch studio", 1);
        consolePrint("  config save <key> <value> - save a setting", 1);
        consolePrint("  config read <key> - read a setting", 1);
        consolePrint("  join <ip:port>    - join a server via IP", 1);
        consolePrint("  map <name>        - set the map", 1);
        consolePrint("  hostport <port>   - set the server port", 1);
        consolePrint("  maxplayers <n>    - set the player limit", 1);
        consolePrint("  altip <ip|clear>  - set/clear the alternate server IP", 1);
        consolePrint("  notifications on|off - server notifications", 1);
        consolePrint("  upnp on|off       - UPnP port forwarding", 1);
        consolePrint("  proxy on|off      - the local web proxy", 1);
        consolePrint("  extensions        - list loaded extensions", 1);
        consolePrint("  dependencies      - check launcher dependencies", 1);
        consolePrint("  documentation     - client script documentation", 1);
        consolePrint("  sdk               - open the SDK", 1);
        consolePrint("  neofetch          - system information", 1);
        consolePrint("  genarrays         - generate AES key arrays", 1);
        consolePrint("  reset             - reset the launcher config", 1);
        consolePrint("  reload            - reload extensions", 1);
        consolePrint("  clear             - clear the console", 1);
        consolePrint("  sh <cmd>          - run a shell command", 1);
        consolePrint("  exit              - close the console", 1);
        return;
    }
    if (lower == "exit" || lower == "quit") {
        m_appClose = true;
        close();
        return;
    }
    if (lower == "clear") {
        m_console->clear();
        return;
    }
    if (lower.startsWith("join")) {
        if (args.size() < 2) {
            consolePrint("Usage: join <ip:port>", 2);
            return;
        }
        m_ui.user->saveSetting("IP", args[1]);
        GuiShared::launchGame(m_ui, ScriptType_Client, false, this);
        return;
    }
    if (lower.startsWith("map")) {
        if (args.size() < 2) {
            consolePrint("Usage: map <map name>", 2);
            return;
        }
        m_ui.user->saveSetting("Map", args[1]);
        m_ui.user->saveSetting("MapPath", Paths::mapsDir + "/" + args[1]);
        consolePrint("Map set to: " + m_ui.user->readSetting("Map"), 4);
        return;
    }
    if (lower.startsWith("hostport")) {
        if (args.size() < 2 || !args[1].toInt()) {
            consolePrint("Usage: hostport <port>", 2);
            return;
        }
        m_ui.user->saveSettingInt("RobloxPort", args[1].toInt());
        consolePrint("RobloxPort set to: " + QString::number(args[1].toInt()), 4);
        return;
    }
    if (lower.startsWith("maxplayers")) {
        if (args.size() < 2 || !args[1].toInt()) {
            consolePrint("Usage: maxplayers <count>", 2);
            return;
        }
        m_ui.user->saveSettingInt("PlayerLimit", args[1].toInt());
        consolePrint("PlayerLimit set to: " + QString::number(args[1].toInt()), 4);
        return;
    }
    if (lower.startsWith("altip")) {
        if (args.size() < 2) {
            consolePrint("Usage: altip <ip|clear>", 2);
            return;
        }
        const QString value = args[1] == "clear" ? QString() : args[1];
        m_ui.user->saveSetting("AlternateServerIP", value);
        consolePrint("AlternateServerIP set to: "
                     + (value.isEmpty() ? "(cleared)" : value), 4);
        return;
    }
    if (lower.startsWith("notifications")) {
        const bool on = args.size() < 2 || args[1].toLower() != "off";
        m_ui.user->saveSettingBool("ShowServerNotifications", on);
        consolePrint(QString("ShowServerNotifications: ") + (on ? "on" : "off"), 4);
        return;
    }
    if (lower.startsWith("upnp")) {
        const bool on = args.size() >= 2 && args[1].toLower() == "on";
        m_ui.user->saveSettingBool("UPnP", on);
        consolePrint(QString("UPnP: ") + (on ? "on (ports open when a server launches)" : "off"), 4);
        return;
    }
    if (lower.startsWith("proxy")) {
        if (args.size() >= 2 && args[1].toLower() == "on") {
            m_ui.user->saveSettingBool("WebProxyEnabled", true);
            const bool ok = WebProxy::start(true);
            consolePrint(ok ? "Web Proxy started on port 6171."
                             : "Web Proxy failed to start.", ok ? 3 : 2);
            return;
        }
        if (args.size() >= 2 && args[1].toLower() == "off") {
            m_ui.user->saveSettingBool("WebProxyEnabled", false);
            WebProxy::stop(true);
            consolePrint("Web Proxy stopped.", 3);
            return;
        }
        consolePrint("Usage: proxy on|off", 2);
        return;
    }
    if (lower == "extensions" || lower == "reload") {
        static Extensions::Manager manager;
        if (lower == "reload") {
            manager.unload();
            consolePrint("Reloading Extensions...", 4);
        }
        const QStringList list = manager.load(Paths::novetusExts);
        if (list.isEmpty()) {
            consolePrint("No extensions found.", 5);
        } else {
            for (const QString &ext : list) {
                consolePrint("Loaded extension " + ext, 3);
            }
        }
        return;
    }
    if (lower == "dependencies") {
        // CheckDependencies parity: the Windows VC++ runtime check becomes a
        // Wine dependency check for the client executables.
        QProcess probe;
        probe.start(LinuxCompat::wineCommandProgram(),
                    QStringList(LinuxCompat::wineCommandPrefixArgs()) << "--version");
        probe.waitForFinished(10000);
        consolePrint(probe.state() == QProcess::NotRunning && probe.exitCode() == 0
                     ? "Dependencies: wine found (" + QString::fromUtf8(probe.readAllStandardOutput()).trimmed() + ")"
                     : "Dependencies: wine not found - clients cannot launch without it.",
                     probe.exitCode() == 0 ? 3 : 2);
        return;
    }
    if (lower == "documentation") {
        DocsWindow docs(this);
        docs.exec();
        return;
    }
    if (lower == "sdk") {
        SdkWindow w(this);
        w.exec();
        return;
    }
    if (lower == "neofetch") {
        consolePrint(kBanner, 1);
        consolePrint("    " + m_ui.user->readSetting("PlayerName", true) + "@novetus", 1);
        consolePrint("    ----------------------------", 1);
        consolePrint("    Novetus version: " + GuiShared::productVersion(), 1);
        consolePrint("    Novetus path: " + Paths::basePath, 1);
        consolePrint("    Distribution Codename: " + GuiShared::branchName(), 1);
        consolePrint("    Selected Client: " + m_ui.user->readSetting("SelectedClient"), 1);
        consolePrint("    Configuration: " + Paths::configDir + "/config.json", 1);
        consolePrint("    OS: " + QSysInfo::prettyProductName(), 1);
        return;
    }
    if (lower == "genarrays") {
        // SecurityFuncs.GenArrays parity
        QString keys = "public static byte[] aeskey = new byte[32] { ";
        for (int i = 0; i < 32; i++) {
            keys += QString::number(QRandomGenerator::system()->bounded(0, 255))
                    + (i == 31 ? "" : ", ");
        }
        keys += " };";
        QString ivs = "public static byte[] aesiv = new byte[16] { ";
        for (int i = 0; i < 16; i++) {
            ivs += QString::number(QRandomGenerator::system()->bounded(0, 255))
                   + (i == 15 ? "" : ", ");
        }
        ivs += " };";
        consolePrint(keys, 4);
        consolePrint(ivs, 4);
        return;
    }
    if (lower == "reset") {
        if (QMessageBox::question(this, "Novetus - Reset",
                                  "Reset the launcher configuration?")
            == QMessageBox::Yes) {
            FileManagement::resetConfigValues(m_ui.user, m_ui.user->readSettingInt("LauncherStyle"),
                                              m_ui.user->readSetting("SelectedClient"),
                                              m_ui.user->readSetting("Map"));
            FileManagement::resetCustomizationValues(m_ui.custom);
            consolePrint("Configuration reset.", 3);
        }
        return;
    }
    if (lower.startsWith("sh")) {
        if (args.size() < 2) {
            consolePrint("Usage: sh <command>", 2);
            return;
        }
        QProcess::startDetached("/bin/sh", {"-c", cmd.section(' ', 1)});
        consolePrint("Command sent to the shell.", 4);
        return;
    }
    if (lower == "client") {
        GuiShared::launchGame(m_ui, ScriptType_Client, false, this);
        return;
    }
    if (lower == "solo") {
        GuiShared::launchGame(m_ui, ScriptType_Solo, false, this);
        return;
    }
    if (lower == "outfitview") {
        GuiShared::launchGame(m_ui, ScriptType_OutfitView, false, this);
        return;
    }
    if (lower.contains("server")) {
        const bool no3d = args.size() > 1 && args[1].toLower() == "no3d";
        GuiShared::launchGame(m_ui, ScriptType_Server, no3d, this);
        return;
    }
    if (lower.contains("studio")) {
        const bool nomap = args.size() > 1 && args[1].toLower() == "nomap";
        Q_UNUSED(nomap);
        GuiShared::launchGame(m_ui, ScriptType_Studio, false, this);
        return;
    }
    if (lower.contains("config")) {
        if (args.size() >= 4 && (args[1] == "save" || args[1] == "savesetting" || args[1] == "saveval")) {
            m_ui.user->saveSetting(args[2], cmd.section(' ', 3));
            consolePrint("Value of '" + args[2] + "': " + m_ui.user->readSetting(args[2]), 4);
            consolePrint("Please restart any applications related to this value to reload it.", 4);
        } else if (args.size() >= 3 && (args[1] == "read" || args[1] == "readsetting" || args[1] == "readval")) {
            consolePrint("Value of '" + args[2] + "': " + m_ui.user->readSetting(args[2]), 4);
        } else {
            consolePrint("Please specify 'save', 'read', 'load', or 'reset'.", 2);
            consolePrint("If specifying 'save', please provide the name and value for the setting you wish to save.", 2);
        }
        return;
    }
    consolePrint("Unknown command. Type 'help' to list commands.", 2);
}
