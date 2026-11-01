#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include "core/script.h"
#include "core/launcher.h"

// QPushButton drawn with the original Novetus button art
// (normal / _hover / _click PNGs from the Stylish launcher).
class ImageButton : public QPushButton
{
    Q_OBJECT
public:
    ImageButton(const QString &assetBase, const QString &toolTip, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
    bool event(QEvent *e) override;

private:
    QPixmap m_normal, m_hover, m_pressed;
    bool m_hovering = false;
    bool m_pressedNow = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshClients();
    void launchGame(ScriptType type);

private:
    ScriptContext makeContext(ScriptType type);
    void logLine(const QString &msg);
    void pickSplash();

    Config *m_userConfig = nullptr;
    Config *m_customConfig = nullptr;

    QTreeWidget *m_mapsTree = nullptr;
    QComboBox *m_clientBox = nullptr;
    QLabel *m_selectedMap = nullptr;
    QLabel *m_selectedClient = nullptr;
    QPlainTextEdit *m_console = nullptr;
    QPlainTextEdit *m_serverInfo = nullptr;
    QLabel *m_splash = nullptr;
    QTabWidget *m_tabs = nullptr;
};
