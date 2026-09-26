#pragma once

#include <QObject>
#include <QCryptographicHash>
#include <QFile>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QVersionNumber>
#include <optional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

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
    explicit UpdateManager(QObject *parent = nullptr,
                           QNetworkAccessManager *network = nullptr,
                           const QString &downloadRoot = {});
    ~UpdateManager() override;
    void checkForUpdates(bool silent = false);
    void downloadUpdate(const ReleaseInfo &release);
    void cancel();
    bool isBusy() const;
    void preserveInstaller();

    static std::optional<ReleaseInfo> parseRelease(const QByteArray &json,
                                                  const QVersionNumber &currentVersion,
                                                  QString *errorMessage = nullptr);
    static QByteArray parseSha256(const QByteArray &contents, QString *errorMessage = nullptr);
    static bool verifySha256(const QString &filePath, const QByteArray &expectedHex);

signals:
    void checkingChanged(bool checking);
    void busyChanged(bool busy);
    void updateAvailable(const ReleaseInfo &release);
    void upToDate();
    void downloadProgress(qint64 received, qint64 total);
    void readyToInstall(const QString &installerPath);
    void failed(const QString &message, bool silent);

private:
    enum class State { Idle, Checking, Checksum, Downloading, Ready };
    void request(const QUrl &url);
    void consumeData();
    void finishRequest();
    void fail(const QString &message);
    void reset();
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    State m_state = State::Idle;
    bool m_silent = false;
    bool m_checkingReleasePage = false;
    QString m_downloadRoot;
    ReleaseInfo m_release;
    std::unique_ptr<QTemporaryDir> m_directory;
    QFile m_file;
    QByteArray m_response;
    QByteArray m_expectedHash;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
};
