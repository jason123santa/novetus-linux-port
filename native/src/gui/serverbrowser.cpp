#include "serverbrowser.h"

#include <QPushButton>
#include <QLabel>
#include <QSslSocket>
#include <QMessageBox>
#include <QHeaderView>
#include <QRegularExpression>
#include <QBrush>

#include "core/paths.h"
#include "core/crypto.h"
#include "core/launcher.h"
#include "fit.h"

ServerBrowser::ServerBrowser(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Server Browser");
    setWindowIcon(QIcon(":/assets/extracted/NovetusLauncher.Properties.Resources/N.png"));
    resize(577, 456);
    m_ui = GuiShared::createConfigs();

    auto *masterLabel = new QLabel("Master Server Address:", this);
    masterLabel->setGeometry(12, 13, 117, 13);
    m_master = new QLineEdit(this);
    m_master->setGeometry(135, 10, 323, 20);
    Fit::labelThen(masterLabel, m_master, 8);
    m_master->setText(m_ui.user->readSetting("ServerBrowserServerAddress"));
    auto *refresh = new QPushButton(this);
    refresh->setGeometry(464, 8, 25, 23);
    refresh->setIcon(QIcon(":/assets/refresh.png"));
    connect(refresh, &QPushButton::clicked, this, &ServerBrowser::refreshServers);

    m_list = new QTreeWidget(this);
    m_list->setGeometry(10, 37, 555, 408);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({"Name", "Client", "Version"});
    m_list->header()->setSectionResizeMode(QHeaderView::Fixed);
    m_list->setColumnWidth(0, 284);
    m_list->setColumnWidth(1, 75);
    m_list->setColumnWidth(2, 110);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this] { joinGame(); });

    auto *join = new QPushButton("JOIN GAME", this);
    join->setGeometry(495, 8, 75, 23);
    connect(join, &QPushButton::clicked, this, &ServerBrowser::joinGame);

    refreshServers();
}

void ServerBrowser::refreshServers()
{
    if (m_loading) {
        return;
    }
    const QString master = m_master->text().trimmed();
    m_ui.user->saveSetting("ServerBrowserServerAddress", master);
    m_servers.clear();
    m_list->clear();
    if (master.isEmpty()) {
        return;
    }
    m_loading = true;
    setWindowTitle("Server Browser (Loading Servers...)");
    fetchServerList(master);
}

void ServerBrowser::fetchServerList(const QString &masterAddress)
{
    // The original loads http://<master>/serverlist.txt; the server redirects
    // to https. Use a raw TLS request WITHOUT a User-Agent header: the master
    // 403s any request that carries one (the .NET WebClient sends none).
    QString host = masterAddress;
    int port = 443;
    const int colon = host.lastIndexOf(':');
    if (colon > 0 && host.mid(colon + 1).toInt() > 0) {
        port = host.mid(colon + 1).toInt();
        host = host.left(colon);
    }

    if (m_socket) {
        m_socket->deleteLater();
    }
    m_response.clear();
    m_socket = new QSslSocket(this);
    connect(m_socket, &QSslSocket::encrypted, this, [this, host] {
        // HTTP/1.0: no chunked encoding, close-delimited body. An empty
        // User-Agent value is accepted by the master.
        const QByteArray req =
            "GET /serverlist.txt HTTP/1.0\r\n"
            "Host: " + host.toUtf8() + "\r\n"
            "User-Agent: \r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n\r\n";
        m_socket->write(req);
    });
    connect(m_socket, &QSslSocket::readyRead, this, [this] {
        m_response += m_socket->readAll();
    });
    connect(m_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError e) {
        // RemoteHostClosedError is the normal HTTP/1.0 "Connection: close"
        // termination; the disconnected handler parses the response.
        if (e == QAbstractSocket::RemoteHostClosedError || !m_loading) return;
        m_loading = false;
        setWindowTitle("Server Browser");
        QMessageBox::warning(this, "Novetus - Error",
                             "Cannot load the server list (" + m_socket->errorString() + ").");
    });
    connect(m_socket, &QSslSocket::disconnected, this, [this] {
        if (!m_loading) return;
        m_loading = false;
        setWindowTitle("Server Browser");
        const int statusEnd = m_response.indexOf("\r\n");
        const QByteArray status = m_response.left(statusEnd);
        const int bodyStart = m_response.indexOf("\r\n\r\n");
        const int httpStatus = status.contains(" 200 ") ? 200
            : (status.mid(9, 3).toInt());
        if (httpStatus != 200) {
            QString hint;
            if (httpStatus == 403) {
                hint = "\n\nThe master server refused access (HTTP 403). If this persists, "
                       "the master may have stopped serving the classic serverlist.txt feed.";
            }
            QMessageBox::warning(this, "Novetus - Error",
                                 "Cannot load the server list (HTTP " + QByteArray::number(httpStatus)
                                 + ")." + hint);
            return;
        }
        parseServerList(m_response.mid(bodyStart + 4));
    });
    m_socket->connectToHostEncrypted(host, port);
}

void ServerBrowser::parseServerList(const QByteArray &body)
{
    const QStringList lines = QString::fromUtf8(body)
                                  .split(QRegularExpression("\\r\\n|\\n"), Qt::SkipEmptyParts);
    const QString currentVersion = GuiShared::productVersion();
    for (const QString &line : lines) {
        // LoadServerInfoFromFile: <id>|<base64 blob>; the blob decodes to
        // b64(name)|b64(ip)|b64(port)|b64(client)|b64(version), and
        // ServerBrowserDef base64-decodes each field once more.
        const QStringList a2 = line.split('|');
        if (a2.size() < 2) continue;
        const QString text3 = Crypto::base64Decode(a2[1]);
        const QStringList a3 = text3.split('|');
        if (a3.size() < 5) continue;
        ServerEntry e;
        e.name = Crypto::base64Decode(a3[0]);
        e.ip = Crypto::base64Decode(a3[1]);
        e.port = Crypto::base64Decode(a3[2]).toInt();
        e.client = Crypto::base64Decode(a3[3]);
        e.version = Crypto::base64Decode(a3[4]);
        if (e.name.isEmpty() || e.client.isEmpty() || e.ip.isEmpty() || e.port <= 0) continue;
        if (e.ip == "localhost" || e.ip == "127.0.0.1") continue;
        m_servers.push_back(e);
        auto *item = new QTreeWidgetItem(m_list, {e.name, e.client, e.version});
        if (e.version != currentVersion) {
            item->setForeground(2, QBrush(Qt::red));
        }
    }
    if (m_servers.isEmpty()) {
        auto *item = new QTreeWidgetItem(m_list, {"No servers found."});
        item->setFlags(Qt::NoItemFlags);
    }
}

void ServerBrowser::joinGame()
{
    auto *item = m_list->currentItem();
    if (!item || m_servers.isEmpty()) {
        QMessageBox::warning(this, "Novetus - Error", "Select a server before joining it.");
        return;
    }
    for (const ServerEntry &e : m_servers) {
        if (e.name == item->text(0)) {
            ScriptContext ctx = GuiShared::makeContext(m_ui, ScriptType_Client,
                                                       e.ip + ":" + QString::number(e.port));
            ctx.clientName = e.client;
            ctx.currentServerIP = e.ip;
            ctx.currentServerPort = e.port;
            QString err;
            Launcher launcher(ctx);
            if (!launcher.launch(ScriptType_Client, false, /*nomap*/ true, QString(), &err)) {
                QMessageBox::critical(this, "Novetus - Error",
                                      "Cannot join server (" + err + ").");
            }
            return;
        }
    }
}
