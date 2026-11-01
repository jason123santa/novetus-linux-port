#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include "guishared.h"

// 1:1 port of NovetusLauncher.LauncherFormCompact (417x366, horizontal tabs).
class CompactWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit CompactWindow(QWidget *parent = nullptr);

private:
    GuiShared::Ui m_ui;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_selectedClient = nullptr;
    QLabel *m_selectedMap = nullptr;
    QLabel *m_splash = nullptr;
};
