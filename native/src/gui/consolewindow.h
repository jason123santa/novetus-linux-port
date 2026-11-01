#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QLineEdit>
#include "guishared.h"

// 1:1 port of NovetusLauncher.NovetusConsole (869x417, FixedToolWindow).
class ConsoleWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ConsoleWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void consolePrint(const QString &text, int type = 1);
    void processCommands(const QString &cmd);
    void printBanner();

    GuiShared::Ui m_ui;
    QPlainTextEdit *m_console = nullptr;
    QLineEdit *m_command = nullptr;
    bool m_helpMode = false;
    bool m_appClose = false;
};
