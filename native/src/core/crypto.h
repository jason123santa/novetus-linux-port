#pragma once
#include <QString>

// Port of SecurityFuncs.Decode/Encode (AES-256-CBC, PKCS7, UTF-16LE plaintext,
// base64 wrapper) and Util.GenerateMD5.
namespace Crypto {

// Hardcoded key store (Novetus.Core.GlobalKeyStore)
extern const unsigned char AES_KEY[32];
extern const unsigned char AES_IV[16];

QString decode(const QString &encoded);   // base64 -> AES decrypt -> UTF-16LE text
QString encode(const QString &plainText); // UTF-16LE -> AES encrypt -> base64
QString base64Decode(const QString &s);
QString base64Encode(const QString &s);

QString md5File(const QString &path);     // uppercase hex, no dashes

} // namespace Crypto
