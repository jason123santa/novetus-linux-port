#pragma once
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QStackedWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QButtonGroup>
#include "core/script.h"
#include "core/launcher.h"

// 1:1 port of NovetusLauncher.LauncherFormExtended (739x421, pure WinForms).
// Geometry is taken verbatim from the decompiled InitializeComponent.
class ExtendedWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ExtendedWindow(QWidget *parent = nullptr);

private:
    // construction helpers (positions in WinForms client coordinates)
    QFrame *makePanel(QWidget *parent, int x, int y, int w, int h);
    QPushButton *makeButton(QWidget *parent, int x, int y, int w, int h, const QString &text,
                            float ptSize = 8.25f, std::function<void()> onClick = nullptr);
    QLabel *makeLabel(QWidget *parent, int x, int y, int w, int h, const QString &text,
                      float ptSize = 8.25f, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter);
    QLineEdit *makeLine(QWidget *parent, int x, int y, int w, int h, const QString &text = QString());
    QPlainTextEdit *makeEdit(QWidget *parent, int x, int y, int w, int h);

    ScriptContext makeContext(ScriptType type);
    bool doLaunch(ScriptType type, bool no3d = false);
    void logLine(const QString &msg);
    void pickSplash();
    void loadClients();
    void loadMaps(const QString &filter = QString());
    void loadSavedLists();
    void loadServerInfo();
    void syncOptionsFromConfig();

    Config *m_userConfig = nullptr;
    Config *m_customConfig = nullptr;

    // header (panel3)
    QLineEdit *m_nameBox = nullptr;
    QLineEdit *m_idBox = nullptr;
    QLabel *m_splash = nullptr;
    QLabel *m_selectedClientLabel = nullptr;
    QLabel *m_selectedMapLabel = nullptr;

    // sidebar
    QList<QPushButton *> m_sideButtons;
    QStackedWidget *m_pages = nullptr;

    // PLAY page
    QLineEdit *m_addressBox = nullptr;

    // HOST page
    QPlainTextEdit *m_serverInfo = nullptr;
    QSpinBox *m_portBox = nullptr;
    QSpinBox *m_limitBox = nullptr;
    QCheckBox *m_upnpBox = nullptr;
    QCheckBox *m_notifBox = nullptr;
    QLineEdit *m_browserName = nullptr;
    QLineEdit *m_browserAddress = nullptr;

    // VERSIONS page
    QListWidget *m_clientList = nullptr;
    QLabel *m_clientDesc = nullptr;
    QPlainTextEdit *m_clientWarning = nullptr;

    // MAPS page
    QLineEdit *m_searchBar = nullptr;
    QTreeWidget *m_mapsTree = nullptr;
    QPlainTextEdit *m_mapDesc = nullptr;

    // SAVED page
    QListWidget *m_serversList = nullptr;
    QListWidget *m_portsList = nullptr;
    QLabel *m_currentIp = nullptr;
    QLabel *m_currentPort = nullptr;

    // OPTIONS page
    QCheckBox *m_discordBox = nullptr;
    QCheckBox *m_localPlayBox = nullptr;
    QCheckBox *m_minimizeBox = nullptr;
    QCheckBox *m_webProxyBox = nullptr;
    QComboBox *m_styleBox = nullptr;

    // server info / online share
    QString m_externalIP = QStringLiteral("localhost");
    QNetworkAccessManager *m_net = nullptr;
    void fetchExternalIP();
};
