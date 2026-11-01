#pragma once
#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QVector>
#include "core/config.h"

class QNetworkAccessManager;
class QNetworkReply;

// 1:1 port of NovetusLauncher CharacterCustomizationExtended (665x434):
// sidebar navigation + BODY/HATS/HEADS/FACES/T-SHIRTS/SHIRTS/PANTS/EXTRA/OTHER
// pages at the decompiled coordinates. Auto-saves to config_customization.json.
class CustomizationWindow : public QDialog
{
    Q_OBJECT
public:
    explicit CustomizationWindow(QWidget *parent = nullptr);

private:
    struct PartColorEntry {
        QString name;
        int id = 0;
        QColor color;
    };
    struct Slot {
        QString key;         // config key (Hat1, Face, ...)
        QString title;
        QString dirName;     // subdirectory under charcustom
        QString defaultItem; // Reset target (without .rbxm)
        QListWidget *list = nullptr;
        QLabel *preview = nullptr;
        QTextEdit *desc = nullptr;
        QLineEdit *idBox = nullptr;      // custom ID row (faces-pants)
        QComboBox *typeBox = nullptr;
        QString *customUrl = nullptr;    // provider URL state
    };

    void buildUi();
    void loadPalette();
    void buildBodyPage();
    void buildHatPages();
    void buildItemPages();
    void buildOtherPage();
    void fillSlot(Slot &slot);
    void selectSlotItem(Slot &slot, const QString &item);
    void randomizeSlot(Slot &slot);
    void resetSlot(Slot &slot);
    QString itemFile(const Slot &slot, const QString &base, const QString &suffix) const;
    QString userConfigName() const;
    QColor colorForId(int colorId) const;
    void applyColorToPart(int partIndex, const PartColorEntry &entry);
    void refreshPartButtons();
    void resetColors();
    void randomizeColors();
    void loadOutfitFile();
    void selectPart(int partIndex);

    Config *m_custom = nullptr;
    Config *m_userConfig = nullptr;
    QVector<PartColorEntry> m_palette;

    // sidebar
    QList<QPushButton *> m_sideButtons;
    QStackedWidget *m_pages = nullptr;

    // BODY page
    QStringList m_partKeys;   // HeadColorID, TorsoColorID, ...
    QStringList m_partNames;  // Head, Torso, Left Arm, ...
    QVector<QPushButton *> m_partButtons; // indexed by part 0..5
    QString m_selectedPart = QStringLiteral("Head");
    QListWidget *m_colorList = nullptr;
    QLabel *m_selectedPartValue = nullptr;
    QString m_faceUrl, m_tshirtUrl, m_shirtUrl, m_pantsUrl;
    QNetworkAccessManager *m_net = nullptr;

    // HATS nested pages
    QStackedWidget *m_hatPages = nullptr;
    Slot m_hatSlots[3];

    // item pages
    Slot m_headSlot, m_faceSlot, m_tshirtSlot, m_shirtSlot, m_pantsSlot, m_extraSlot;
    QCheckBox *m_showHats = nullptr;

    // OTHER page
    QLabel *m_iconPreview = nullptr;
    QLabel *m_iconName = nullptr;
    QLineEdit *m_iconUrl = nullptr;
    QLineEdit *m_appearanceUrl = nullptr;
};
