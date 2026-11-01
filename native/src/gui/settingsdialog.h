#pragma once
#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include "guishared.h"

// 1:1 port of NovetusLauncher.LauncherFormSettings (426x177).
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    GuiShared::Ui m_ui;
    QComboBox *m_graphicsMode = nullptr;
    QComboBox *m_quality = nullptr;
    QComboBox *m_priority = nullptr;
    QCheckBox *m_newGui = nullptr;
};
