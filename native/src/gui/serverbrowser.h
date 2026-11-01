#pragma once
#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include "guishared.h"

class QSslSocket;

// 1:1 port of NovetusLauncher.ServerBrowser (577x456).
class ServerBrowser : public QDialog
{
    Q_OBJECT
public:
    explicit ServerBrowser(QWidget *parent = nullptr);

private slots:
    void refreshServers();
    void joinGame();

private:
    struct ServerEntry {
        QString name, ip, client, version;
        int port = 0;
    };
    // Minimal HTTP/HTTPS GET with no User-Agent, matching the original
    // .NET WebClient (the master 403s any request that carries one).
    void fetchServerList(const QString &masterAddress);
    void parseServerList(const QByteArray &body);

    GuiShared::Ui m_ui;
    QLineEdit *m_master = nullptr;
    QTreeWidget *m_list = nullptr;
    QList<ServerEntry> m_servers;
    QSslSocket *m_socket = nullptr;
    QByteArray m_response;
    bool m_loading = false;
};
