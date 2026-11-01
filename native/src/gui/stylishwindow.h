#pragma once
#include <QMainWindow>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QCheckBox>
#include "guishared.h"

// 1:1 port of NovetusLauncher.LauncherFormStylish (707x419) + the WPF
// LauncherFormStylishInterface, rebuilt from the decompiled XAML.
class StylishWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit StylishWindow(QWidget *parent = nullptr);

private:
    GuiShared::Ui m_ui;
    QLineEdit *m_userName = nullptr;
    QLineEdit *m_userId = nullptr;
    QLineEdit *m_address = nullptr;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_mapsTree = nullptr;
    QPlainTextEdit *m_mapsDesc = nullptr;
    QListWidget *m_clientList = nullptr;
    QPlainTextEdit *m_clientWarning = nullptr;
    QPlainTextEdit *m_clientDesc = nullptr;
    QPlainTextEdit *m_serverInfo = nullptr;
    QLineEdit *m_portBox = nullptr;
    QLineEdit *m_maxPlayers = nullptr;
    QCheckBox *m_upnp = nullptr;
    QCheckBox *m_notif = nullptr;
    QCheckBox *m_discord = nullptr;
    QCheckBox *m_minimize = nullptr;
    QCheckBox *m_webProxy = nullptr;
};
