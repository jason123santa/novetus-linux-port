#include "customizationwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "core/paths.h"
#include "core/launcher.h"
#include "guishared.h"

namespace {

// Body parts in C# ChangeColorOfPart order.
struct PartDef { const char *key; const char *name; };
const PartDef kParts[6] = {
    {"HeadColorID", "Head"},
    {"TorsoColorID", "Torso"},
    {"LeftArmColorID", "Left Arm"},
    {"RightArmColorID", "Right Arm"},
    {"LeftLegColorID", "Left Leg"},
    {"RightLegColorID", "Right Leg"},
};

// C# '06 Color Presets (head, torso, larm, rarm, lleg, rleg)
const int kPresets[8][6] = {
    {24, 194, 24, 24, 119, 119},
    {24, 101, 24, 24, 9, 9},
    {24, 23, 24, 24, 119, 119},
    {24, 101, 24, 24, 119, 119},
    {24, 45, 24, 24, 119, 119},
    {106, 194, 106, 106, 119, 119},
    {106, 119, 106, 106, 119, 119},
    {9, 194, 9, 9, 119, 119},
};

QColor parseRgb(const QString &rgb) // "[242, 243, 243]"
{
    QString body = rgb;
    body.remove('[').remove(']');
    const QStringList parts = body.split(',');
    if (parts.size() >= 3) {
        return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(),
                      parts[2].trimmed().toInt());
    }
    return QColor(163, 162, 165);
}

} // namespace

CustomizationWindow::CustomizationWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Novetus - Character Customization (native Linux)");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    setFixedSize(665, 434);

    m_custom = new Config(QStringLiteral("Customization"),
                          Paths::configDir + "/config_customization.json");
    m_custom->load();
    m_userConfig = new Config(QStringLiteral("Config"),
                              Paths::configDir + "/config.json");
    m_userConfig->load();
    loadPalette();
    buildUi();
    refreshPartButtons();
    selectPart(0);
}

void CustomizationWindow::loadPalette()
{
    m_palette.clear();
    QFile f(Paths::configDir + "/PartColors.json");
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "PartColors.json not found";
        return;
    }
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        PartColorEntry e;
        e.name = o["ColorName"].toString();
        // PartColors.json stores ColorID as a string ("24")
        e.id = QString::number(o["ColorID"].toDouble()).toInt();
        if (e.id == 0) e.id = o["ColorID"].toString().toInt();
        e.color = parseRgb(o["ColorRGB"].toString());
        m_palette.push_back(e);
    }
}

QColor CustomizationWindow::colorForId(int colorId) const
{
    for (const PartColorEntry &e : m_palette) {
        if (e.id == colorId) return e.color;
    }
    return QColor(163, 162, 165);
}

QString CustomizationWindow::userConfigName() const
{
    return m_userConfig->readSetting("PlayerName", true);
}

void CustomizationWindow::buildUi()
{
    for (int i = 0; i < 6; i++) {
        m_partKeys << kParts[i].key;
        m_partNames << kParts[i].name;
    }
    m_partButtons.resize(6);

    // ---- sidebar (panel1: 2,3,85x302) ----
    auto *sidebar = new QFrame(this);
    sidebar->setGeometry(2, 3, 85, 302);
    const char *nav[9] = {"BODY", "HATS", "HEADS", "FACES", "T-SHIRTS",
                          "SHIRTS", "PANTS", "EXTRA", "OTHER"};
    for (int i = 0; i < 9; i++) {
        QPushButton *b = new QPushButton(QString::fromLatin1(nav[i]), sidebar);
        b->setGeometry(3, 3 + i * 26, 75, 23);
        b->setCheckable(true);
        b->setFont(QFont("Microsoft Sans Serif", 8.25f));
        m_sideButtons.push_back(b);
        connect(b, &QPushButton::clicked, this, [this, i] {
            for (int j = 0; j < m_sideButtons.size(); j++) {
                m_sideButtons[j]->setChecked(j == i);
            }
            m_pages->setCurrentIndex(i);
        });
    }
    auto *preview3d = new QPushButton("3D PREVIEW", sidebar);
    preview3d->setGeometry(3, 247, 75, 47);
    preview3d->setFont(QFont("Microsoft Sans Serif", 8.25f));
    connect(preview3d, &QPushButton::clicked, this, [this] {
        GuiShared::launchGame(GuiShared::createConfigs(), ScriptType_OutfitView, false, this);
    });
    m_sideButtons[0]->setChecked(true);

    // ---- content (panel2: 93,3,568x329) ----
    auto *panel2 = new QFrame(this);
    panel2->setGeometry(93, 3, 568, 329);
    m_pages = new QStackedWidget(panel2);
    m_pages->setGeometry(3, 3, 557, 319);

    buildBodyPage();
    buildHatPages();
    buildItemPages();
    buildOtherPage();
    m_pages->setCurrentIndex(0);

    // ---- bottom (panel3: 110,359,535x62) ----
    auto *panel3 = new QFrame(this);
    panel3->setGeometry(110, 359, 535, 62);
    auto *h1 = new QPushButton("HAT #1", panel3);
    h1->setGeometry(159, 4, 56, 23);
    auto *h2 = new QPushButton("HAT #2", panel3);
    h2->setGeometry(221, 4, 69, 23);
    auto *h3 = new QPushButton("HAT #3", panel3);
    h3->setGeometry(296, 4, 56, 23);
    connect(h1, &QPushButton::clicked, this, [this] {
        m_pages->setCurrentIndex(1);
        m_hatPages->setCurrentIndex(0);
    });
    connect(h2, &QPushButton::clicked, this, [this] {
        m_pages->setCurrentIndex(1);
        m_hatPages->setCurrentIndex(1);
    });
    connect(h3, &QPushButton::clicked, this, [this] {
        m_pages->setCurrentIndex(1);
        m_hatPages->setCurrentIndex(2);
    });
    auto *randAll = new QPushButton("Randomize all 3", panel3);
    randAll->setGeometry(3, 33, 253, 29);
    connect(randAll, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < 3; i++) randomizeSlot(m_hatSlots[i]);
    });
    auto *resetAll = new QPushButton("Reset all 3", panel3);
    resetAll->setGeometry(269, 33, 263, 29);
    connect(resetAll, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < 3; i++) resetSlot(m_hatSlots[i]);
    });
}

void CustomizationWindow::buildBodyPage()
{
    QWidget *page = new QWidget();
    m_pages->addWidget(page);

    // "Body Parts" group (6,6,187x281)
    auto *groupBody = new QGroupBox(QStringLiteral("Body Parts"), page);
    groupBody->setGeometry(6, 6, 187, 281);

    auto *backdrop = new QLabel(groupBody);
    backdrop->setGeometry(0, 16, 187, 265);
    const QString backdropFile = Paths::dataDir + "/CharacterBackdrop.png";
    if (QFileInfo::exists(backdropFile)) {
        backdrop->setPixmap(QPixmap(backdropFile).scaled(
            backdrop->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    backdrop->lower();

    // figure buttons at decompiled coordinates (relative to the group)
    const struct { int x, y, w, h; int part; } figure[6] = {
        {65, 26, 58, 59, 0},   // head
        {55, 90, 77, 87, 1},   // torso
        {12, 90, 37, 87, 2},   // left arm
        {138, 90, 37, 87, 3},  // right arm
        {55, 182, 37, 87, 4},  // left leg
        {95, 182, 37, 87, 5},  // right leg
    };
    for (const auto &f : figure) {
        QPushButton *b = new QPushButton(groupBody);
        b->setGeometry(f.x, f.y, f.w, f.h);
        b->setToolTip(QString::fromLatin1(kParts[f.part].name));
        connect(b, &QPushButton::clicked, this, [this, f] { selectPart(f.part); });
        m_partButtons[f.part] = b;
    }

    // "Body Colors" group (199,6,341x195)
    auto *groupColors = new QGroupBox(QStringLiteral("Body Colors"), page);
    groupColors->setGeometry(199, 6, 341, 195);

    m_colorList = new QListWidget(groupColors);
    m_colorList->setGeometry(6, 16, 329, 161);
    for (const PartColorEntry &e : m_palette) {
        QPixmap swatch(16, 16);
        swatch.fill(e.color);
        QListWidgetItem *it = new QListWidgetItem(QIcon(swatch), e.name);
        it->setData(Qt::UserRole, e.id);
        m_colorList->addItem(it);
    }
    connect(m_colorList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int id = item->data(Qt::UserRole).toInt();
        const int partIndex = m_partKeys.indexOf(m_selectedPart);
        for (const PartColorEntry &e : m_palette) {
            if (e.id == id) { applyColorToPart(partIndex, e); break; }
        }
    });
    auto *note = new QLabel(
        QStringLiteral("Some colors may show up as grey in-game."), groupColors);
    note->setGeometry(6, 177, 329, 15);

    // '06 Color Presets group (411,210,109x68) — 8 hardcoded presets
    auto *presets = new QGroupBox(QStringLiteral("'06 Color Presets"), page);
    presets->setGeometry(411, 210, 109, 68);
    for (int i = 0; i < 8; i++) {
        QPushButton *b = new QPushButton(QString::number(i + 1), presets);
        b->setGeometry(6 + (i % 4) * 25, 16 + (i / 4) * 26, 24, 23);
        b->setFont(QFont("Microsoft Sans Serif", 6.75f));
        const int *preset = kPresets[i];
        connect(b, &QPushButton::clicked, this, [this, preset] {
            for (int p = 0; p < 6; p++) {
                for (const PartColorEntry &e : m_palette) {
                    if (e.id == preset[p]) { applyColorToPart(p, e); break; }
                }
            }
        });
    }

    // SELECTED PART + Randomize/Reset Colors
    auto *selLabel = new QLabel("SELECTED PART:", page);
    selLabel->setGeometry(230, 211, 105, 16);
    selLabel->setFont(QFont("Microsoft Sans Serif", 8.25f));
    m_selectedPartValue = new QLabel(m_selectedPart, page);
    m_selectedPartValue->setGeometry(330, 210, 80, 16);
    auto *randBtn = new QPushButton("Randomize Colors", page);
    randBtn->setGeometry(230, 229, 89, 49);
    randBtn->setFont(QFont("Microsoft Sans Serif", 7));
    connect(randBtn, &QPushButton::clicked, this, [this] { randomizeColors(); });
    auto *resetBtn = new QPushButton("Reset Colors", page);
    resetBtn->setGeometry(320, 229, 90, 49);
    resetBtn->setFont(QFont("Microsoft Sans Serif", 7));
    connect(resetBtn, &QPushButton::clicked, this, [this] { resetColors(); });
}

void CustomizationWindow::buildHatPages()
{
    QWidget *page = new QWidget();
    m_pages->addWidget(page);
    m_hatPages = new QStackedWidget(page);
    m_hatPages->setGeometry(3, 0, 543, 247);

    const char *titles[3] = {"HAT 1", "HAT 2", "HAT 3"};
    for (int i = 0; i < 3; i++) {
        QWidget *hatPage = new QWidget();
        m_hatPages->addWidget(hatPage);
        Slot &slot = m_hatSlots[i];
        slot.key = QString("Hat%1").arg(i + 1);
        slot.title = QString("HAT %1").arg(i + 1);
        slot.dirName = "hats";
        slot.defaultItem = "NoHat";

        auto *list = new QListWidget(hatPage);
        list->setGeometry(6, 6, 244, 212);
        auto *titleLabel = new QLabel(QString::fromLatin1(titles[i]), hatPage);
        titleLabel->setGeometry(293, 126, 40, 13);
        auto *preview = new QLabel(hatPage);
        preview->setGeometry(339, 8, 131, 131);
        preview->setAlignment(Qt::AlignCenter);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setStyleSheet("background: #202020;");
        auto *desc = new QTextEdit(hatPage);
        desc->setReadOnly(true);
        desc->setGeometry(279, 145, 250, 73);
        slot.list = list;
        slot.preview = preview;
        slot.desc = desc;

        connect(list, &QListWidget::currentTextChanged, this, [this, i](const QString &item) {
            selectSlotItem(m_hatSlots[i], item);
        });
        fillSlot(slot);
    }
}

void CustomizationWindow::buildItemPages()
{
    // HEADS
    {
        QWidget *page = new QWidget();
        m_pages->addWidget(page);
        Slot &slot = m_headSlot;
        slot.key = "Head";
        slot.title = "HEADS";
        slot.dirName = "heads";
        slot.defaultItem = "DefaultHead";
        auto *list = new QListWidget(page);
        list->setGeometry(3, 3, 253, 238);
        auto *preview = new QLabel(page);
        preview->setGeometry(343, 15, 134, 134);
        preview->setAlignment(Qt::AlignCenter);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setStyleSheet("background: #202020;");
        auto *desc = new QTextEdit(page);
        desc->setReadOnly(true);
        desc->setGeometry(283, 155, 263, 86);
        auto *randBtn = new QPushButton("Randomize", page);
        randBtn->setGeometry(3, 248, 253, 42);
        auto *resetBtn = new QPushButton("Reset", page);
        resetBtn->setGeometry(283, 248, 263, 42);
        slot.list = list;
        slot.preview = preview;
        slot.desc = desc;
        connect(list, &QListWidget::currentTextChanged, this, [this](const QString &item) {
            selectSlotItem(m_headSlot, item);
        });
        connect(randBtn, &QPushButton::clicked, this, [this] { randomizeSlot(m_headSlot); });
        connect(resetBtn, &QPushButton::clicked, this, [this] { resetSlot(m_headSlot); });
        fillSlot(slot);
    }

    // FACES (+ custom ID row), T-SHIRTS, SHIRTS, PANTS — same page shape
    const struct { const char *key; const char *title; const char *dir;
                   const char *def; const char *idLabel; } defs[4] = {
        {"Face", "FACES", "faces", "DefaultFace", "Custom Face ID"},
        {"TShirt", "T-SHIRTS", "tshirts", "NoTShirt", "Custom T-Shirt ID"},
        {"Shirt", "SHIRTS", "shirts", "NoShirt", "Custom Shirt ID"},
        {"Pants", "PANTS", "pants", "NoPants", "Custom Pants ID"},
    };
    for (const auto &def : defs) {
        QWidget *page = new QWidget();
        m_pages->addWidget(page);
        Slot *slot = (def.key == QString("Face")) ? &m_faceSlot
                   : (def.key == QString("TShirt")) ? &m_tshirtSlot
                   : (def.key == QString("Shirt")) ? &m_shirtSlot : &m_pantsSlot;
        slot->key = def.key;
        slot->title = def.title;
        slot->dirName = def.dir;
        slot->defaultItem = def.def;
        auto *list = new QListWidget(page);
        list->setGeometry(3, 3, 253, 186);
        auto *preview = new QLabel(page);
        preview->setGeometry(343, 15, 134, 134);
        preview->setAlignment(Qt::AlignCenter);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setStyleSheet("background: #202020;");
        auto *desc = new QTextEdit(page);
        desc->setReadOnly(true);
        desc->setGeometry(283, 155, 263, 86);
        auto *randBtn = new QPushButton("Randomize", page);
        randBtn->setGeometry(3, 248, 253, 42);
        auto *resetBtn = new QPushButton("Reset", page);
        resetBtn->setGeometry(283, 248, 263, 42);
        // custom ID row (ContentProvider): setting becomes provider.URL + id
        auto *idBox = new QLineEdit(page);
        idBox->setGeometry(3, 195, 100, 20);
        auto *idLabel = new QLabel(QString::fromLatin1(def.idLabel), page);
        idLabel->setGeometry(111, 198,
                             def.key == QString("Shirt") ? 80
                             : def.key == QString("Pants") ? 86 : 90, 13);
        auto *typeBox = new QComboBox(page);
        typeBox->setGeometry(3, 221, 253, 21);
        for (const QString &name : GuiShared::contentProviderNames()) {
            typeBox->addItem(name);
        }
        slot->list = list;
        slot->preview = preview;
        slot->desc = desc;
        slot->idBox = idBox;
        slot->typeBox = typeBox;
        slot->customUrl = (def.key == QString("Face")) ? &m_faceUrl
                        : (def.key == QString("TShirt")) ? &m_tshirtUrl
                        : (def.key == QString("Shirt")) ? &m_shirtUrl : &m_pantsUrl;
        connect(list, &QListWidget::currentTextChanged, this, [this, slot](const QString &item) {
            selectSlotItem(*slot, item);
        });
        connect(randBtn, &QPushButton::clicked, this, [this, slot] { randomizeSlot(*slot); });
        connect(resetBtn, &QPushButton::clicked, this, [this, slot] { resetSlot(*slot); });
        // C# FaceIDBox_TextChanged / FaceTypeBox_SelectedIndexChanged parity
        connect(idBox, &QLineEdit::textChanged, this, [this, slot](const QString &text) {
            QSignalBlocker blocker(slot->list);
            resetSlot(*slot); // selects the default item (C# parity)
            if (!text.trimmed().isEmpty()) {
                m_custom->saveSetting(slot->key, *slot->customUrl + text);
            }
        });
        connect(typeBox, &QComboBox::currentIndexChanged, this, [this, slot](int index) {
            if (index < 0) return;
            *slot->customUrl = GuiShared::contentProviders()[index].url;
            if (!slot->idBox->text().trimmed().isEmpty()) {
                m_custom->saveSetting(slot->key, *slot->customUrl + slot->idBox->text());
            }
        });
        // restore custom-id state from the saved setting (ApplyContentProvider parity)
        const QString saved = m_custom->readSetting(slot->key);
        if (saved.contains("http")) {
            for (int i = 0; i < GuiShared::contentProviders().size(); i++) {
                const QString url = GuiShared::contentProviders()[i].url;
                if (saved.startsWith(url)) {
                    typeBox->setCurrentIndex(i);
                    idBox->setText(saved.mid(url.size()));
                    break;
                }
            }
        }
        fillSlot(*slot);
    }

    // EXTRA (+ Show Hats)
    {
        QWidget *page = new QWidget();
        m_pages->addWidget(page);
        Slot &slot = m_extraSlot;
        slot.key = "Extra";
        slot.title = "EXTRA";
        slot.dirName = "custom";
        slot.defaultItem = "NoExtra";
        auto *list = new QListWidget(page);
        list->setGeometry(3, 3, 253, 225);
        auto *preview = new QLabel(page);
        preview->setGeometry(343, 15, 134, 134);
        preview->setAlignment(Qt::AlignCenter);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setStyleSheet("background: #202020;");
        auto *desc = new QTextEdit(page);
        desc->setReadOnly(true);
        desc->setGeometry(283, 155, 263, 86);
        auto *randBtn = new QPushButton("Randomize", page);
        randBtn->setGeometry(3, 248, 253, 42);
        auto *resetBtn = new QPushButton("Reset", page);
        resetBtn->setGeometry(283, 248, 263, 42);
        m_showHats = new QCheckBox("Show Hats", page);
        m_showHats->setGeometry(3, 230, 79, 17);
        slot.list = list;
        slot.preview = preview;
        slot.desc = desc;
        connect(list, &QListWidget::currentTextChanged, this, [this](const QString &item) {
            selectSlotItem(m_extraSlot, item);
        });
        connect(randBtn, &QPushButton::clicked, this, [this] { randomizeSlot(m_extraSlot); });
        connect(resetBtn, &QPushButton::clicked, this, [this] { resetSlot(m_extraSlot); });
        connect(m_showHats, &QCheckBox::toggled, this, [this] { fillSlot(m_extraSlot); });
        fillSlot(slot);
    }
}

void CustomizationWindow::buildOtherPage()
{
    QWidget *page = new QWidget();
    m_pages->addWidget(page);

    auto *selLabel = new QLabel("Selected Icon:", page);
    selLabel->setGeometry(86, 9, 111, 19);
    m_iconName = new QLabel(m_custom->readSetting("Icon"), page);
    m_iconName->setGeometry(159, 9, 56, 19);

    // BC / TBC / OBC badge buttons (background images from the original resx)
    const struct { int x; const char *label; const char *value; const char *badge; } badges[3] = {
        {7, "BC", "BC", "button52_BackgroundImage.png"},
        {113, "TBC", "TBC", "button53_BackgroundImage.png"},
        {219, "OBC", "OBC", "button54_BackgroundImage.png"},
    };
    for (const auto &b : badges) {
        QPushButton *btn = new QPushButton(page);
        btn->setGeometry(b.x, 37, 100, 111);
        btn->setIcon(QIcon(":/assets/extracted/CharacterCustomizationCompact/"
                           + QString::fromLatin1(b.badge)));
        btn->setIconSize(QSize(88, 96));
        connect(btn, &QPushButton::clicked, this, [this, b] {
            m_iconUrl->clear();
            m_custom->saveSetting("Icon", QString::fromLatin1(b.value));
            m_iconName->setText(QString::fromLatin1(b.value));
        });
    }

    m_iconPreview = new QLabel(page);
    m_iconPreview->setGeometry(336, 63, 88, 88);
    m_iconPreview->setAlignment(Qt::AlignCenter);
    m_iconPreview->setFrameShape(QFrame::StyledPanel);
    m_iconPreview->setStyleSheet("background: #202020;");

    auto *customLabel = new QLabel("Custom Icon", page);
    customLabel->setGeometry(389, 45, 101, 13);
    auto *browse = new QPushButton("Browse Local Image...", page);
    browse->setGeometry(439, 63, 92, 35);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, "Open icon .png", QString(), "Portable Network Graphics image (*.png)");
        if (file.isEmpty()) return;
        // IconLoader parity: install to extradirIcons/<PlayerName>.png
        QDir().mkpath(Paths::dataPath + "/charcustom/custom/icons");
        const QString dest = Paths::dataPath + "/charcustom/custom/icons/"
                             + userConfigName() + ".png";
        QFile::remove(dest);
        QFile::copy(file, dest);
        m_iconPreview->setPixmap(QPixmap(dest).scaled(
            m_iconPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });

    auto *urlLabel = new QLabel("Online URL", page);
    urlLabel->setGeometry(451, 115, 62, 13);
    m_iconUrl = new QLineEdit(page);
    m_iconUrl->setGeometry(428, 131, 111, 20);
    connect(m_iconUrl, &QLineEdit::textChanged, this, [this](const QString &text) {
        // LoadRemoteIcon parity
        if (text.isEmpty() || text.contains("BC") || text.contains("TBC")
            || text.contains("OBC")) {
            m_custom->saveSetting("Icon", "NBC");
            m_iconName->setText("NBC");
            const QString localIcon = Paths::dataPath + "/charcustom/custom/icons/"
                                      + userConfigName() + ".png";
            if (QFileInfo::exists(localIcon)) {
                m_iconPreview->setPixmap(QPixmap(localIcon).scaled(
                    m_iconPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_iconPreview->setPixmap(QPixmap());
                m_iconPreview->setText("(no custom icon)");
            }
            return;
        }
        if (text.contains("http://") || text.contains("https://")) {
            m_custom->saveSetting("Icon", text);
            m_iconName->setText("NBC");
            // preview: fetch asynchronously like LoadRemoteIcon
            if (!m_net) {
                m_net = new QNetworkAccessManager(this);
            }
            QNetworkReply *reply = m_net->get(QNetworkRequest(QUrl(text)));
            connect(reply, &QNetworkReply::finished, this, [this, reply] {
                reply->deleteLater();
                QPixmap pm;
                if (reply->error() == QNetworkReply::NoError) {
                    pm.loadFromData(reply->readAll());
                }
                if (!pm.isNull()) {
                    m_iconPreview->setPixmap(pm.scaled(
                        m_iconPreview->size(), Qt::KeepAspectRatio,
                        Qt::SmoothTransformation));
                } else {
                    m_iconPreview->setPixmap(QPixmap());
                    m_iconPreview->setText("(load failed)");
                }
            });
        }
    });

    auto *disableBtn = new QPushButton("Disable Icon/Enable Custom Icon", page);
    disableBtn->setGeometry(7, 151, 314, 23);
    connect(disableBtn, &QPushButton::clicked, this, [this] {
        m_iconUrl->clear();
        m_custom->saveSetting("Icon", "NBC");
        m_iconName->setText("NBC");
    });

    auto *note = new QLabel(QStringLiteral(
        "NOTE: The icon will only function in a client with a custom scoreboard "
        "(I.E 2011M). Icons from a local image will load only for the player, "
        "while online images will show up for all players."), page);
    note->setGeometry(43, 182, 465, 44);
    note->setWordWrap(true);

    auto *divider = new QLabel(page);
    divider->setGeometry(3, 230, 543, 2);
    divider->setFrameShape(QFrame::Box);

    auto *appearanceLabel = new QLabel(QStringLiteral(
        "Character Appearance URL (for clients that require it)"), page);
    appearanceLabel->setGeometry(11, 238, 323, 19);
    m_appearanceUrl = new QLineEdit(page);
    m_appearanceUrl->setGeometry(40, 260, 258, 20);
    m_appearanceUrl->setText(m_custom->readSetting("CharacterID"));
    connect(m_appearanceUrl, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_custom->saveSetting("CharacterID", t);
    });

    auto *outfitLabel = new QLabel("Outfit Settings", page);
    outfitLabel->setGeometry(390, 238, 73, 13);
    auto *saveBtn = new QPushButton("Save Outfit", page);
    saveBtn->setGeometry(350, 258, 73, 22);
    connect(saveBtn, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "Novetus - Outfit Saved", "Outfit Saved!");
    });
    auto *loadBtn = new QPushButton("Load Outfit", page);
    loadBtn->setGeometry(429, 258, 73, 22);
    connect(loadBtn, &QPushButton::clicked, this, [this] { loadOutfitFile(); });

    // initial icon preview: custom icon if installed, else NoExtra.png fallback
    const QString localIcon = Paths::dataPath + "/charcustom/custom/icons/"
                              + userConfigName() + ".png";
    const QString fallback = Paths::dataPath + "/charcustom/custom/NoExtra.png";
    const QString show = QFileInfo::exists(localIcon) ? localIcon : fallback;
    if (QFileInfo::exists(show)) {
        m_iconPreview->setPixmap(QPixmap(show).scaled(
            m_iconPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// ---- slot logic (CharacterCustomizationShared parity) ----

QString CustomizationWindow::itemFile(const Slot &slot, const QString &base,
                                      const QString &suffix) const
{
    const QString inSlot = Paths::dataPath + "/charcustom/" + slot.dirName
                           + "/" + base + suffix;
    if (QFileInfo::exists(inSlot)) return inSlot;
    // the Extra slot can reference hats (%hat4d% points at the hats directory)
    if (slot.key == "Extra") {
        const QString inHats = Paths::dataPath + "/charcustom/hats/" + base + suffix;
        if (QFileInfo::exists(inHats)) return inHats;
    }
    return QString();
}

void CustomizationWindow::fillSlot(Slot &slot)
{
    const QString dir = Paths::dataPath + "/charcustom/" + slot.dirName;
    const QString current = m_custom->readSetting(slot.key);
    slot.list->clear();
    for (const QFileInfo &fi : QDir(dir).entryInfoList({"*.rbxm"}, QDir::Files, QDir::Name)) {
        slot.list->addItem(fi.fileName());
    }
    // Extra also lists hats when "Show Hats" is on (C# hatsinextra)
    if (&slot == &m_extraSlot && m_showHats && m_showHats->isChecked()) {
        QDir hats(Paths::dataPath + "/charcustom/hats");
        for (const QFileInfo &fi : hats.entryInfoList({"*.rbxm"}, QDir::Files, QDir::Name)) {
            if (fi.fileName() != "NoHat.rbxm") slot.list->addItem(fi.fileName());
        }
    }
    QListWidgetItem *target = nullptr;
    const auto found = slot.list->findItems(current, Qt::MatchExactly);
    if (!found.isEmpty()) {
        target = found.first();
    } else if (!slot.defaultItem.isEmpty()) {
        const auto def = slot.list->findItems(slot.defaultItem + ".rbxm", Qt::MatchExactly);
        if (!def.isEmpty()) target = def.first();
    }
    if (!target && slot.list->count() > 0) target = slot.list->item(0);
    if (target) {
        QSignalBlocker blocker(slot.list);
        slot.list->setCurrentItem(target);
    }
    selectSlotItem(slot, target ? target->text() : QString());
}

void CustomizationWindow::selectSlotItem(Slot &slot, const QString &item)
{
    if (item.isEmpty()) return;
    m_custom->saveSetting(slot.key, item);

    QString base = item;
    base.chop(5); // ".rbxm"
    const QString png = itemFile(slot, base, ".png");
    if (!png.isEmpty()) {
        slot.preview->setPixmap(QPixmap(png).scaled(
            slot.preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        slot.preview->setPixmap(QPixmap());
        slot.preview->setText("(no preview)");
    }
    const QString descFile = itemFile(slot, base, "_desc.txt");
    if (!descFile.isEmpty()) {
        QFile f(descFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            slot.desc->setPlainText(QString::fromUtf8(f.readAll()));
            return;
        }
    }
    slot.desc->setPlainText(item);
}

void CustomizationWindow::randomizeSlot(Slot &slot)
{
    if (slot.list->count() == 0) return;
    const int index = int(QRandomGenerator::global()->bounded(uint(slot.list->count())));
    QSignalBlocker blocker(slot.list);
    slot.list->setCurrentRow(index);
    selectSlotItem(slot, slot.list->item(index)->text());
    // C# parity: randomize clears the custom ID box and resets the provider
    if (slot.idBox) {
        QSignalBlocker idBlock(slot.idBox);
        slot.idBox->clear();
    }
    if (slot.typeBox) {
        QSignalBlocker typeBlock(slot.typeBox);
        slot.typeBox->setCurrentIndex(0);
        *slot.customUrl = GuiShared::contentProviders()[0].url;
    }
}

void CustomizationWindow::resetSlot(Slot &slot)
{
    const auto def = slot.list->findItems(slot.defaultItem + ".rbxm", Qt::MatchExactly);
    if (slot.idBox) {
        QSignalBlocker idBlock(slot.idBox);
        slot.idBox->clear();
    }
    if (slot.typeBox) {
        QSignalBlocker typeBlock(slot.typeBox);
        slot.typeBox->setCurrentIndex(0);
        *slot.customUrl = GuiShared::contentProviders()[0].url;
    }
    if (!def.isEmpty()) {
        QSignalBlocker blocker(slot.list);
        slot.list->setCurrentItem(def.first());
        selectSlotItem(slot, def.first()->text());
    }
}

void CustomizationWindow::selectPart(int partIndex)
{
    m_selectedPart = QString::fromLatin1(kParts[partIndex].name);
    m_selectedPartValue->setText(m_selectedPart);
    for (int i = 0; i < m_partButtons.size(); i++) {
        if (QPushButton *b = m_partButtons[i]) {
            b->setChecked(i == partIndex);
        }
    }
    // highlight the part's current color in the palette
    const int colorId = m_custom->readSettingInt(m_partKeys[partIndex]);
    for (int i = 0; i < m_colorList->count(); i++) {
        if (m_colorList->item(i)->data(Qt::UserRole).toInt() == colorId) {
            m_colorList->setCurrentRow(i);
            m_colorList->scrollToItem(m_colorList->item(i));
            break;
        }
    }
}

void CustomizationWindow::applyColorToPart(int partIndex, const PartColorEntry &entry)
{
    if (partIndex < 0 || partIndex >= 6) return;
    m_custom->saveSettingInt(m_partKeys[partIndex], entry.id);
    m_partButtons[partIndex]->setStyleSheet(
        QString("QPushButton { background-color: rgb(%1,%2,%3); color: %4; }")
            .arg(entry.color.red()).arg(entry.color.green()).arg(entry.color.blue())
            .arg((entry.color.lightness() < 128) ? "white" : "black"));
}

void CustomizationWindow::refreshPartButtons()
{
    for (int i = 0; i < 6; i++) {
        const int id = m_custom->readSettingInt(m_partKeys[i]);
        const QColor color = colorForId(id);
        if (m_partButtons[i]) {
            m_partButtons[i]->setStyleSheet(
                QString("QPushButton { background-color: rgb(%1,%2,%3); color: %4;"
                        " border: 1px solid rgb(80,80,80); border-radius: 3px; }")
                    .arg(color.red()).arg(color.green()).arg(color.blue())
                    .arg((color.lightness() < 128) ? "white" : "black"));
        }
    }
}

void CustomizationWindow::resetColors()
{
    // C# CustomizationConfig.DefaultColors
    const int defaults[6] = {24, 23, 24, 24, 119, 119};
    for (int i = 0; i < 6; i++) {
        m_custom->saveSettingInt(m_partKeys[i], defaults[i]);
    }
    refreshPartButtons();
}

void CustomizationWindow::randomizeColors()
{
    for (int i = 0; i < 6; i++) {
        const int index = int(QRandomGenerator::global()->bounded(uint(m_palette.size())));
        m_custom->saveSettingInt(m_partKeys[i], m_palette[index].id);
    }
    refreshPartButtons();
}

void CustomizationWindow::loadOutfitFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, "Load config_customization.json", Paths::configDir,
        "Novetus config_customization files (*.json)");
    if (file.isEmpty()) return;
    // C# LoadAllSettings: replace the customization file wholesale, then reload.
    QFile src(file);
    if (!src.open(QIODevice::ReadOnly)) return;
    QFile dst(Paths::configDir + "/config_customization.json");
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    dst.write(src.readAll());
    dst.close();
    m_custom->load();
    refreshPartButtons();
    for (int i = 0; i < 3; i++) fillSlot(m_hatSlots[i]);
    fillSlot(m_headSlot);
    fillSlot(m_faceSlot);
    fillSlot(m_tshirtSlot);
    fillSlot(m_shirtSlot);
    fillSlot(m_pantsSlot);
    fillSlot(m_extraSlot);
    QMessageBox::information(this, "Novetus - Outfit Loaded", "Outfit Loaded!");
}
