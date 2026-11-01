#pragma once
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QtQml/QJSValue>
#include <QVector>

class QJSEngine;

// Port of Novetus.Core.ExtensionManager / IExtension for the native port.
// The original compiles C# scripts at runtime; here extensions are JavaScript
// files (data/extensions/webproxy/*.js) evaluated with QJSEngine, exposing
// the same hooks the web proxy needs:
//   function info()      -> { name, version, author }   (IExtension parity)
//   function onLoad()                                    (OnExtensionLoad)
//   function onUnload()                                  (OnExtensionUnload)
//   function isValidURL(path, host) -> bool   (IWebProxyExtension)
//   function onRequest(path, host)  -> { status, body } | null | "invalid"
namespace Extensions {

struct Response {
    bool valid = true;
    int status = 404;
    QByteArray body;
};

class Manager
{
public:
    ~Manager();

    // LoadExtensions: evaluates every *.js in dirPath. Returns the loaded
    // extension info strings ("Name vVersion by Author").
    QStringList load(const QString &dirPath);

    void unload();

    bool isEmpty() const { return m_loaded.isEmpty(); }
    QStringList loadedInfo() const { return m_loaded; }

    // Called when the proxy starts (IWebProxyExtension.OnProxyStart parity).
    void onProxyStart();

    // Runs the extension chain for a request. Returns false when no
    // extension claimed the URL (caller responds 404); *resp holds the
    // outcome ("invalid" -> valid=false -> 500).
    bool onRequest(const QString &path, const QString &host, Response *resp);

private:
    QJSEngine *m_engine = nullptr;
    QStringList m_loaded;
    QVector<QJSValue> m_extensions;
};

} // namespace Extensions
