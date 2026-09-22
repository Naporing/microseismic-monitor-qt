#pragma once

#include <QObject>
#include <QUrl>
#include <QVersionNumber>
#include <optional>

struct ReleaseInfo
{
    QVersionNumber version;
    QString tagName;
    QString releaseName;
    QString notes;
    QUrl installerUrl;
    QUrl checksumUrl;
    QString installerFileName;
};
Q_DECLARE_METATYPE(ReleaseInfo)

class UpdateManager : public QObject
{
    Q_OBJECT
public:
    static std::optional<ReleaseInfo> parseRelease(const QByteArray &json,
                                                  const QVersionNumber &currentVersion,
                                                  QString *errorMessage = nullptr);
    static QByteArray parseSha256(const QByteArray &contents, QString *errorMessage = nullptr);
    static bool verifySha256(const QString &filePath, const QByteArray &expectedHex);
};
