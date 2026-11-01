#include "extensions.h"

#include <QtQml/QJSEngine>
#include <QtQml/QJSValue>
#include <QDir>
#include <QFile>
#include <QVector>

namespace Extensions {

namespace {

// Turn a JS result into a Response. A null/undefined result means the
// extension claimed the URL but produced no body (404 like the original's
// GenericResponse); the string "invalid" maps to the 500 handler.
bool toResponse(const QJSValue &result, Response *resp)
{
    if (result.isString() && result.toString() == "invalid") {
        resp->valid = false;
        resp->status = 500;
        return true;
    }
    if (result.isObject()) {
        resp->valid = true;
        resp->status = result.property("status").toInt();
        if (resp->status == 0) resp->status = 200;
        const QJSValue body = result.property("body");
        resp->body = body.toString().toUtf8();
        return true;
    }
    resp->valid = true;
    resp->status = 404;
    resp->body.clear();
    return true;
}

} // namespace

Manager::~Manager()
{
    unload();
    delete m_engine;
}

QStringList Manager::load(const QString &dirPath)
{
    unload();
    m_loaded.clear();
    m_extensions.clear();
    if (!QDir(dirPath).exists()) {
        return m_loaded; // "No extensions found."
    }
    const QFileInfoList files = QDir(dirPath).entryInfoList({"*.js"}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        return m_loaded;
    }
    if (!m_engine) {
        m_engine = new QJSEngine();
    }
    for (const QFileInfo &fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QJSValue result = m_engine->evaluate(QString::fromUtf8(f.readAll()), fi.fileName());
        if (result.isError()) {
            continue; // "Failed to load script" parity
        }
        const QJSValue info = result.property("info");
        QString name = "Unnamed Object", version = "1.0.0", author = "Unknown";
        if (info.isObject()) {
            name = info.property("name").toString();
            version = info.property("version").toString();
            author = info.property("author").toString();
        }
        m_loaded << name + " v" + version + " by " + author;
        m_extensions << result;
        const QJSValue onLoad = result.property("onLoad");
        if (onLoad.isCallable()) {
            onLoad.call();
        }
    }
    return m_loaded;
}

void Manager::unload()
{
    for (const QJSValue &ext : m_extensions) {
        const QJSValue onUnload = ext.property("onUnload");
        if (onUnload.isCallable()) onUnload.call();
    }
    m_extensions.clear();
    m_loaded.clear();
}

void Manager::onProxyStart()
{
    // IWebProxyExtension.OnProxyStart parity: notify every proxy extension.
    for (const QJSValue &ext : m_extensions) {
        const QJSValue onStart = ext.property("onProxyStart");
        if (onStart.isCallable()) onStart.call();
    }
}

bool Manager::onRequest(const QString &path, const QString &host, Response *resp)
{
    for (const QJSValue &ext : m_extensions) {
        const QJSValue validUrl = ext.property("isValidURL");
        if (!validUrl.isCallable()) continue;
        QJSValueList args;
        args << path << host;
        if (!validUrl.call(args).toBool()) continue;

        const QJSValue onReq = ext.property("onRequest");
        QJSValueList reqArgs;
        reqArgs << path << host;
        const QJSValue result = onReq.isCallable() ? onReq.call(reqArgs) : QJSValue();
        toResponse(result, resp);
        return true; // claimed
    }
    return false;
}

} // namespace Extensions
