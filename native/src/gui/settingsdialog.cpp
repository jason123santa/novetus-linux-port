#include "settingsdialog.h"

#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <algorithm>
#include "graphicsdialog.h"

#include "core/paths.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Novetus Settings");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    setFixedWidth(560);
    m_ui = GuiShared::createConfigs();

    const QFont labelFont("Microsoft Sans Serif", 8.25f);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);

    // ---- Graphics Options -------------------------------------------------
    auto *graphicsGroup = new QGroupBox("Graphics Options", this);
    auto *graphicsForm = new QFormLayout(graphicsGroup);
    graphicsForm->setContentsMargins(12, 8, 12, 10);
    graphicsForm->setHorizontalSpacing(14);
    graphicsForm->setVerticalSpacing(10);

    m_graphicsMode = new QComboBox(graphicsGroup);
    m_graphicsMode->setFont(labelFont);
    m_graphicsMode->addItems({"Automatic", "OpenGL Stable", "OpenGL Experimental", "DirectX"});
    m_graphicsMode->setCurrentIndex(std::clamp(m_ui.user->readSettingInt("GraphicsMode"), 0, 3));
    m_graphicsMode->setMinimumWidth(180);

    m_quality = new QComboBox(graphicsGroup);
    m_quality->setFont(labelFont);
    m_quality->addItems({"Automatic", "Very Low", "Low", "Medium", "High", "Ultra", "Custom"});
    m_quality->setCurrentIndex(std::clamp(m_ui.user->readSettingInt("QualityLevel"), 0, 6));
    m_quality->setMinimumWidth(180);

    auto *modeLabel = new QLabel("Graphics Mode:", graphicsGroup);
    modeLabel->setFont(labelFont);
    auto *qualityLabel = new QLabel("Graphics Quality:", graphicsGroup);
    qualityLabel->setFont(labelFont);
    graphicsForm->addRow(modeLabel, m_graphicsMode);
    graphicsForm->addRow(qualityLabel, m_quality);

    // hint about what "Custom" unlocks
    auto *hint = new QLabel(
        "Select the Custom quality to adjust per-client graphics options.", graphicsGroup);
    hint->setFont(QFont("Microsoft Sans Serif", 7.5f));
    hint->setStyleSheet("color: #909090;");
    graphicsForm->addRow(QString(), hint);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(10);
    auto *changeCustom = new QPushButton("Change Custom Graphics Options...", graphicsGroup);
    changeCustom->setFont(labelFont);
    connect(changeCustom, &QPushButton::clicked, this, [this] {
        if (m_quality->currentIndex() == 6) {
            GraphicsDialog gfx(m_ui.user, this);
            gfx.exec();
        } else {
            QMessageBox::information(this, "Novetus - Error",
                "You do not have the 'Custom' option selected. Please select it before continuing.");
        }
    });
    auto *resetGfx = new QPushButton("Reset Graphics Options", graphicsGroup);
    resetGfx->setFont(labelFont);
    connect(resetGfx, &QPushButton::clicked, this, [this] {
        m_ui.user->saveSettingInt("QualityLevel", 0);
        m_ui.user->saveSettingInt("GraphicsMode", 0);
        m_graphicsMode->setCurrentIndex(0);
        m_quality->setCurrentIndex(0);
    });
    buttonRow->addStretch(1);
    buttonRow->addWidget(changeCustom);
    buttonRow->addWidget(resetGfx);
    graphicsForm->addRow(QString(), buttonRow);

    root->addWidget(graphicsGroup);

    // ---- Other -------------------------------------------------------------
    auto *otherGroup = new QGroupBox("Other", this);
    auto *otherForm = new QFormLayout(otherGroup);
    otherForm->setContentsMargins(12, 8, 12, 10);
    otherForm->setHorizontalSpacing(14);
    otherForm->setVerticalSpacing(10);

    auto *priorityLabel = new QLabel("Process Priority:", otherGroup);
    priorityLabel->setFont(labelFont);
    m_priority = new QComboBox(otherGroup);
    m_priority->setFont(labelFont);
    m_priority->addItems({"Default", "Below Normal", "Normal", "Above Normal", "High", "RealTime"});
    m_priority->setCurrentIndex(std::clamp(m_ui.user->readSettingInt("Priority"), 0, 5));
    m_priority->setMinimumWidth(180);
    otherForm->addRow(priorityLabel, m_priority);

    m_newGui = new QCheckBox("Toggle 2011L GUI in 2011M", otherGroup);
    m_newGui->setFont(labelFont);
    m_newGui->setChecked(m_ui.user->readSettingBool("NewGUI"));
    otherForm->addRow(QString(), m_newGui);

    root->addWidget(otherGroup);

    root->addStretch(1);

    // ---- footer -------------------------------------------------------------
    auto *info = new QLabel("Close this window to save your settings.", this);
    info->setFont(QFont("Microsoft Sans Serif", 7.5f, QFont::StyleItalic));
    info->setStyleSheet("color: #909090;");
    info->setAlignment(Qt::AlignCenter);
    root->addWidget(info);

    connect(m_graphicsMode, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_ui.user->saveSettingInt("GraphicsMode", i);
    });
    connect(m_quality, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_ui.user->saveSettingInt("QualityLevel", i);
    });
    connect(m_priority, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_ui.user->saveSettingInt("Priority", i);
    });
    connect(m_newGui, &QCheckBox::toggled, this, [this](bool on) {
        m_ui.user->saveSettingBool("NewGUI", on);
    });
}
