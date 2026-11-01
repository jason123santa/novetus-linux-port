#include "graphicsdialog.h"

#include <QPushButton>
#include <QTabWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>

#include "core/paths.h"
#include "core/clientsettings.h"

#include "core/paths.h"
#include "core/clientsettings.h"

namespace {

// Port of CheckIfResolutionString: "800x600" style WxH pairs.
bool checkResolutionString(const QString &text)
{
    const QStringList parts = text.split('x');
    if (parts.size() < 2) return false;
    bool ok1 = false, ok2 = false;
    parts[0].toInt(&ok1);
    parts[1].toInt(&ok2);
    return ok1 && ok2;
}

} // namespace

GraphicsDialog::GraphicsDialog(const Config *userConfig, QWidget *parent)
    : QDialog(parent),
      m_user(userConfig)
{
    setWindowTitle("Custom Graphics Settings");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    setFixedSize(446, 252);
    setStyleSheet("background: #d6d6d6;");

    m_clientName = m_user->readSetting("SelectedClient");
    ClientInfoFile::load(Paths::clientDir, m_clientName, &m_info);

    auto *tabs = new QTabWidget(this);
    tabs->setGeometry(6, 6, 434, 214);

    // ---- tabPage1: Model/Texture ----
    QWidget *page1 = new QWidget();
    tabs->addTab(page1, "Model/Texture");
    auto *levelLabel = new QLabel("Quality Level (2010+) (0 means automatic)", page1);
    levelLabel->setGeometry(8, 10, 196, 30);
    levelLabel->setWordWrap(true);
    m_level = new QSpinBox(page1);
    m_level->setGeometry(214, 13, 152, 20);
    m_level->setRange(0, 21);
    m_level->setAlignment(Qt::AlignCenter);
    // GraphicsLevel_ValueChanged: values above 19 always snap back (the
    // decompiled condition is always true).
    connect(m_level, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (v > 19) m_level->setValue(19);
    });
    auto *meshLabel = new QLabel("Mesh Quality (2007-2009)", page1);
    meshLabel->setGeometry(8, 42, 196, 13);
    m_mesh = new QSpinBox(page1);
    m_mesh->setGeometry(214, 38, 152, 20);
    m_mesh->setRange(0, 999);
    m_mesh->setValue(100);
    m_mesh->setAlignment(Qt::AlignCenter);
    auto *shadingLabel = new QLabel("Shading Quality (2007-2009)", page1);
    shadingLabel->setGeometry(8, 68, 196, 13);
    m_shading = new QSpinBox(page1);
    m_shading->setGeometry(214, 64, 152, 20);
    m_shading->setRange(0, 999);
    m_shading->setValue(100);
    m_shading->setAlignment(Qt::AlignCenter);
    auto *materialLabel = new QLabel("Material Quality/Truss Detail (2009+)", page1);
    materialLabel->setGeometry(8, 88, 196, 28);
    materialLabel->setWordWrap(true);
    m_material = new QComboBox(page1);
    m_material->setGeometry(214, 90, 152, 21);
    m_material->addItems({"Automatic", "Low", "Medium", "High"});

    // ---- tabPage2: Shadows/Anti-Aliasing ----
    QWidget *page2 = new QWidget();
    tabs->addTab(page2, "Shadows/Anti-Aliasing");
    auto *aaLabel = new QLabel("Anti-Aliasing (2008+)", page2);
    aaLabel->setGeometry(8, 25, 196, 13);
    m_aa = new QComboBox(page2);
    m_aa->setGeometry(214, 21, 152, 21);
    m_aa->addItems({"Automatic", "On", "Off"});
    auto *aaSamplesLabel = new QLabel("Anti-Aliasing Samples", page2);
    aaSamplesLabel->setGeometry(8, 51, 196, 13);
    m_aaSamples = new QComboBox(page2);
    m_aaSamples->setGeometry(214, 48, 152, 21);
    m_aaSamples->addItems({"None", "4x", "8x"});
    auto *shadowsLabel = new QLabel("Shadows (2008+)", page2);
    shadowsLabel->setGeometry(8, 75, 196, 13);
    m_shadows2008 = new QComboBox(page2);
    m_shadows2008->setGeometry(214, 75, 152, 21);
    m_shadows2008->addItems({"Automatic", "On", "Off", "Character-Only"});
    auto *shadows2007Label = new QLabel("Shadows (2007)", page2);
    shadows2007Label->setGeometry(8, 101, 196, 13);
    m_shadows2007 = new QComboBox(page2);
    m_shadows2007->setGeometry(214, 102, 152, 21);
    m_shadows2007->addItems({"On", "Off"});
    auto *bevelsLabel = new QLabel("Bevels (2008+)", page2);
    bevelsLabel->setGeometry(8, 121, 196, 13);
    m_bevels = new QComboBox(page2);
    m_bevels->setGeometry(214, 117, 152, 21);
    m_bevels->addItems({"Automatic", "On", "Off"});

    // ---- tabPage3: Resolution/Styling ----
    QWidget *page3 = new QWidget();
    tabs->addTab(page3, "Resolution/Styling");
    auto *windowResLabel = new QLabel("Window Resolution\n(2007-2009)", page3);
    windowResLabel->setGeometry(8, 6, 200, 26);
    m_windowRes = new QLineEdit(page3);
    m_windowRes->setGeometry(218, 7, 168, 20);
    m_windowRes->setAlignment(Qt::AlignCenter);
    auto *fullscreenResLabel = new QLabel("Fullscreen Resolution\n(2007-2009)", page3);
    fullscreenResLabel->setGeometry(8, 35, 200, 26);
    m_fullscreenRes = new QLineEdit(page3);
    m_fullscreenRes->setGeometry(218, 36, 168, 20);
    m_fullscreenRes->setAlignment(Qt::AlignCenter);
    auto *modernResLabel = new QLabel("Overall Resolution (2010+)", page3);
    modernResLabel->setGeometry(8, 68, 200, 13);
    m_modernRes = new QComboBox(page3);
    m_modernRes->setGeometry(218, 65, 168, 21);
    m_modernRes->addItems({
        "0: Automatic", "1: 720x526", "2: 800x600", "3: 1024x600 (wide)",
        "4: 1024x768", "5: 1280x720 (wide)", "6: 1280x768 (wide)", "7: 1152x864",
        "8: 1280x800 (wide)", "9: 1360x768 (wide)", "10: 1280x960", "11: 1280x1024",
        "12: 1440x900 (wide)", "13: 1600x900 (wide)", "14: 1600x1024 (wide)",
        "15: 1600x1200", "16: 1680x1050 (wide)", "17: 1920x1080 (wide)",
        "18: 1920x1200 (wide)"});
    auto *styleLabel = new QLabel("Window Style (Early 2007)", page3);
    styleLabel->setGeometry(8, 96, 200, 13);
    m_style = new QLineEdit(page3);
    m_style->setGeometry(218, 93, 168, 20);
    auto *stylesFolder = new QPushButton("Styles Folder", page3);
    stylesFolder->setGeometry(218, 119, 108, 23);
    connect(stylesFolder, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(Paths::clientDir + "/" + m_clientName + "/Styles"));
    });
    auto *stylesInfo = new QPushButton("Info", page3);
    stylesInfo->setGeometry(332, 119, 54, 23);
    connect(stylesInfo, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "Novetus - Styles Help",
            "Make sure you place the styles you want in the Styles folder in "
            + Paths::clientDir + "/" + m_clientName + "/Styles.\n"
            "If the files are not placed in this directory, they will not be "
            "loaded properly.\nThis client will accept .msstyles and .cjstyles files.");
    });

    // resolution validation (TextChanged parity)
    connect(m_windowRes, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (m_loaded && !checkResolutionString(t)) {
            QMessageBox::warning(this, "Novetus - Error when inputting resolution.",
                "Please input a valid resolution. i.e 800x600 or 1024x768. "
                "Novetus will reset back to the default resolution.");
            m_windowRes->setText("800x600");
        }
    });
    connect(m_fullscreenRes, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (m_loaded && !checkResolutionString(t)) {
            QMessageBox::warning(this, "Novetus - Error when inputting resolution.",
                "Please input a valid resolution. i.e 800x600 or 1024x768. "
                "Novetus will reset back to the default resolution.");
            m_fullscreenRes->setText("1024x768");
        }
    });

    auto *info = new QLabel("Close this window to save your settings.", this);
    info->setGeometry(8, 226, 430, 18);
    info->setAlignment(Qt::AlignCenter);
    info->setFont(QFont("Microsoft Sans Serif", 8.25f, QFont::Bold));
    info->setStyleSheet("color: red;");

    loadValues();
}

void GraphicsDialog::loadValues()
{
    QDomDocument doc;
    if (!ClientSettings::loadClientSettingsXml(m_clientName, doc)) {
        QMessageBox::warning(this, "Novetus - Error",
            "This client does not support setting adjustment through the Novetus Launcher.\n"
            "Try opening this client in Roblox Studio and adjust it through the settings "
            "in Tools -> Settings.");
        close();
        return;
    }

    namespace CS = ClientSettings;
    auto setSpin = [](QSpinBox *box, const QString &value, float fallback) {
        bool ok = false;
        const int v = qRound(value.toFloat(&ok));
        if (ok) box->setValue(v); else box->setEnabled(false);
    };
    setSpin(m_mesh, CS::getRenderSettings(doc, "maxMeshDetail", "float"), 100);
    setSpin(m_shading, CS::getRenderSettings(doc, "maxShadingQuality", "float"), 100);

    // Material Quality: WoodQuality, falling back to TrussDetail
    QString material = CS::getRenderSettings(doc, "WoodQuality", "token");
    bool materialOk = false;
    const int materialIdx = material.toInt(&materialOk);
    if (!materialOk) {
        material = CS::getRenderSettings(doc, "TrussDetail", "token");
        materialOk = !material.isEmpty();
    }
    if (materialOk) {
        m_material->setCurrentIndex(qBound(0, materialIdx, 3));
    } else {
        m_material->setEnabled(false);
    }

    bool ok = false;
    const int aaIdx = CS::getRenderSettings(doc, "Antialiasing", "token").toInt(&ok);
    if (ok) m_aa->setCurrentIndex(qBound(0, aaIdx, 2)); else m_aa->setEnabled(false);

    const int samples = CS::getRenderSettings(doc, "AASamples", "token").toInt(&ok);
    if (ok) {
        m_aaSamples->setCurrentIndex(samples == 4 ? 1 : samples == 8 ? 2 : 0);
    } else {
        m_aaSamples->setEnabled(false);
    }

    const int bevels = CS::getRenderSettings(doc, "Bevels", "token").toInt(&ok);
    if (ok) m_bevels->setCurrentIndex(qBound(0, bevels, 2)); else m_bevels->setEnabled(false);

    const int shadows2008 = CS::getRenderSettings(doc, "Shadow", "token").toInt(&ok);
    if (ok) {
        m_shadows2008->setCurrentIndex(qBound(0, shadows2008, 3));
    } else {
        m_shadows2008->setEnabled(false);
    }

    // Shadows (2007): "Shadows", falling back to "shadows"
    QString shadows2007 = CS::getRenderSettings(doc, "Shadows", "bool");
    bool shadowsOk = !shadows2007.isEmpty();
    if (!shadowsOk) {
        shadows2007 = CS::getRenderSettings(doc, "shadows", "bool");
        shadowsOk = !shadows2007.isEmpty();
    }
    if (shadowsOk) {
        m_shadows2007->setCurrentIndex(shadows2007.compare("true", Qt::CaseInsensitive) == 0 ? 0 : 1);
    } else {
        m_shadows2007->setEnabled(false);
    }

    const QString style = CS::getRenderSettings(doc, "_skinFile", "string");
    if (!style.isEmpty()) {
        m_style->setText(QString(style).remove("Styles/"));
    } else {
        m_style->setEnabled(false);
    }

    const int quality = CS::getRenderSettings(doc, "QualityLevel", "token").toInt(&ok);
    if (ok) m_level->setValue(quality); else m_level->setEnabled(false);

    // Fullscreen: FullscreenSize, falling back to FullscreenSizePreference
    QString fullscreen = CS::getRenderSettings(doc, "FullscreenSize", "Vector2int16");
    bool fullscreenOk = !fullscreen.isEmpty();
    if (!fullscreenOk) {
        fullscreen = CS::getRenderSettings(doc, "FullscreenSizePreference", "Vector2int16");
        fullscreenOk = !fullscreen.isEmpty();
    }
    if (fullscreenOk) m_fullscreenRes->setText(fullscreen);
    else m_fullscreenRes->setEnabled(false);

    // Window: WindowSize, falling back to WindowSizePreference
    QString window = CS::getRenderSettings(doc, "WindowSize", "Vector2int16");
    bool windowOk = !window.isEmpty();
    if (!windowOk) {
        window = CS::getRenderSettings(doc, "WindowSizePreference", "Vector2int16");
        windowOk = !window.isEmpty();
    }
    if (windowOk) m_windowRes->setText(window);
    else m_windowRes->setEnabled(false);

    const int modern = CS::getRenderSettings(doc, "Resolution", "token").toInt(&ok);
    if (ok) m_modernRes->setCurrentIndex(qBound(0, modern, 18));
    else m_modernRes->setEnabled(false);

    m_loaded = true;
}

void GraphicsDialog::save()
{
    if (!m_loaded) return;
    const int aaSamplesIdx = m_aaSamples->currentIndex();
    const int aaSamples = aaSamplesIdx == 1 ? 4 : aaSamplesIdx == 2 ? 8 : 1;
    const bool shadows2007 = m_shadows2007->currentIndex() != 1;
    ClientSettings::applyCustomClientSettings(
        m_info, m_clientName, m_user, m_mesh->value(), m_shading->value(),
        m_material->currentIndex(), m_aa->currentIndex(), aaSamples,
        m_bevels->currentIndex(), m_shadows2008->currentIndex(), shadows2007,
        m_style->text(), m_level->value(), m_windowRes->text(),
        m_fullscreenRes->text(), m_modernRes->currentIndex());
}

void GraphicsDialog::closeEvent(QCloseEvent *event)
{
    save();
    event->accept();
}

void GraphicsDialog::reject()
{
    save();
    QDialog::reject();
}
