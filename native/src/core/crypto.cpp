#include "crypto.h"

#include <QFile>
#include <QCryptographicHash>
#include <QByteArray>

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

namespace Crypto {

const unsigned char AES_KEY[32] = {
    250, 205, 55, 117, 7, 241, 170, 161, 242, 133,
    100, 166, 20, 193, 211, 196, 107, 220, 175, 147,
    219, 160, 196, 124, 116, 141, 233, 234, 238, 156,
    141, 46
};

const unsigned char AES_IV[16] = {
    113, 144, 143, 178, 53, 249, 182, 103, 66, 63,
    200, 71, 112, 12, 183, 117
};

static QByteArray base64DecodeBytes(const QString &s)
{
    return QByteArray::fromBase64(s.toUtf8());
}

static QByteArray aesCrypt(const QByteArray &in, bool encrypt)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    QByteArray out;
    out.reserve(in.size() + 32);
    unsigned char buf[512];
    int len = 0;
    bool ok = false;
    do {
        if (!ctx) break;
        if (EVP_CipherInit_ex(ctx, EVP_aes_256_cbc(), nullptr, AES_KEY, AES_IV, encrypt ? 1 : 0) != 1) break;
        EVP_CIPHER_CTX_set_padding(ctx, 1); // PKCS7, same as .NET default
        out.resize(in.size() + 32);
        int total = 0;
        if (EVP_CipherUpdate(ctx, (unsigned char *)out.data(), &len,
                             (const unsigned char *)in.constData(), in.size()) != 1) break;
        total = len;
        if (EVP_CipherFinal_ex(ctx, (unsigned char *)out.data() + total, &len) != 1) break;
        total += len;
        out.resize(total);
        ok = true;
    } while (false);
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        out.clear();
    }
    return out;
}

QString decode(const QString &encoded)
{
    QByteArray cipher = base64DecodeBytes(encoded);
    QByteArray plain = aesCrypt(cipher, false);
    if (plain.isEmpty()) {
        return QString();
    }
    // .NET Encoding.Unicode = UTF-16LE
    return QString::fromUtf16(reinterpret_cast<const char16_t *>(plain.constData()), plain.size() / 2);
}

QString encode(const QString &plainText)
{
    // UTF-16LE bytes without BOM (Encoding.Unicode.GetBytes)
    const char16_t *u = reinterpret_cast<const char16_t *>(plainText.utf16());
    QByteArray plain(reinterpret_cast<const char *>(u), plainText.size() * 2);
    QByteArray cipher = aesCrypt(plain, true);
    if (cipher.isEmpty()) {
        return QString();
    }
    return cipher.toBase64();
}

QString base64Decode(const QString &s)
{
    return QString::fromUtf8(base64DecodeBytes(s));
}

QString base64Encode(const QString &s)
{
    return s.toUtf8().toBase64();
}

QString md5File(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(&f)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex()).toUpper();
}

} // namespace Crypto
