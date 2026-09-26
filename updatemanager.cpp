#include "updatemanager.h"
#include "appversion.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDateTime>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>

namespace {
const QString releaseBase = "https://github.com/Naporing/microseismic-monitor-qt/releases/download/";
const QUrl latestReleasePage("https://github.com/Naporing/microseismic-monitor-qt/releases/latest");
const QString releaseTagPath = "/Naporing/microseismic-monitor-qt/releases/tag/";

bool validTag(const QString &tag)
{
    static const QRegularExpression pattern("^v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$");
    if (!pattern.match(tag).hasMatch())
        return false;
    qsizetype suffix = 0;
    const auto version = QVersionNumber::fromString(QStringView(tag).mid(1), &suffix);
    return suffix == tag.size() - 1 && version.segmentCount() == 3;
}

bool safeRedirect(const QUrl &url, bool checking)
{
    if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.hasFragment()
        || (url.port() != -1 && url.port() != 443))
        return false;
    if (checking)
        return url.host() == "api.github.com";
    return url.host() == "release-assets.githubusercontent.com"
        || (url.host() == "github.com"
            && url.path().startsWith("/Naporing/microseismic-monitor-qt/releases/download/"));
}

void cleanOldDownloads(const QString &root)
{
    QDir directory(root);
    static const QRegularExpression pattern("^update-v[0-9]+\\.[0-9]+\\.[0-9]+-[a-zA-Z0-9]+$");
    const auto cutoff = QDateTime::currentDateTimeUtc().addDays(-7);
    for (const auto &entry : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
    {
        if (!pattern.match(entry.fileName()).hasMatch() || entry.lastModified() > cutoff)
            continue;
        QDir old(entry.absoluteFilePath());
        for (const auto &file : old.entryInfoList({"SeismicWaveforms-Setup-v*.exe", "SeismicWaveforms-Setup-v*.exe.part"},
                                                 QDir::Files | QDir::NoSymLinks))
            old.remove(file.fileName());
        directory.rmdir(entry.fileName());
    }
}
}

UpdateManager::UpdateManager(QObject *parent, QNetworkAccessManager *network, const QString &downloadRoot)
    : QObject(parent),
      m_network(network ? network : new QNetworkAccessManager(this)),
      m_downloadRoot(downloadRoot.isEmpty()
                         ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("SeismicWaveformsDemo")
                         : downloadRoot)
{
    qRegisterMetaType<ReleaseInfo>();
    if (downloadRoot.isEmpty())
        cleanOldDownloads(m_downloadRoot);
}

UpdateManager::~UpdateManager()
{
    reset();
}

bool UpdateManager::isBusy() const
{
    return m_state != State::Idle;
}

void UpdateManager::reset()
{
    if (m_reply)
    {
        auto *reply = m_reply.data();
        m_reply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    m_file.close();
    m_directory.reset();
    m_response.clear();
    m_state = State::Idle;
    m_checkingReleasePage = false;
}

void UpdateManager::cancel()
{
    const bool checking = m_state == State::Checking;
    reset();
    if (checking)
        emit checkingChanged(false);
    emit busyChanged(false);
}

void UpdateManager::preserveInstaller()
{
    if (m_state == State::Ready && m_directory)
        m_directory->setAutoRemove(false);
}

void UpdateManager::fail(const QString &message)
{
    const bool silent = m_silent;
    cancel();
    emit failed(message, silent);
}

void UpdateManager::checkForUpdates(bool silent)
{
    if (isBusy())
        return;
    m_silent = silent;
    m_state = State::Checking;
    emit busyChanged(true);
    emit checkingChanged(true);
    request(QUrl("https://api.github.com/repos/Naporing/microseismic-monitor-qt/releases/latest"));
}

void UpdateManager::downloadUpdate(const ReleaseInfo &release)
{
    if (isBusy())
        return;
    m_silent = false;
    const QString name = "SeismicWaveforms-Setup-" + release.tagName + ".exe";
    if (!validTag(release.tagName) || release.installerFileName != name
        || release.version != QVersionNumber::fromString(release.tagName.mid(1))
        || release.version <= QVersionNumber::fromString(APP_VERSION)
        || release.installerUrl != QUrl(releaseBase + release.tagName + "/" + name)
        || release.checksumUrl != QUrl(releaseBase + release.tagName + "/" + name + ".sha256"))
    {
        fail("更新安装包信息无效，请重新检查更新。");
        return;
    }
    m_release = release;
    if (!QDir().mkpath(m_downloadRoot))
    {
        fail("无法创建更新下载目录。");
        return;
    }
    m_directory = std::make_unique<QTemporaryDir>(QDir(m_downloadRoot).filePath("update-" + release.tagName + "-XXXXXX"));
    if (!m_directory->isValid())
    {
        fail("无法创建更新临时目录。");
        return;
    }
    m_state = State::Checksum;
    emit busyChanged(true);
    request(release.checksumUrl);
}

void UpdateManager::request(const QUrl &url)
{
    m_response.clear();
    QNetworkRequest request(url);
    const bool releasePage = m_state == State::Checking && m_checkingReleasePage;
    request.setRawHeader("User-Agent", "SeismicWaveformsDemo/" APP_VERSION);
    if (releasePage)
        request.setRawHeader("Accept", "text/html");
    else
        request.setRawHeader("Accept", m_state == State::Checking ? "application/vnd.github+json"
                                                             : "application/octet-stream");
    if (m_state == State::Checking && !releasePage)
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(30000);
    request.setMaximumRedirectsAllowed(5);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         releasePage ? QNetworkRequest::ManualRedirectPolicy
                                     : QNetworkRequest::UserVerifiedRedirectPolicy);
    m_reply = releasePage ? m_network->head(request) : m_network->get(request);
    m_reply->setReadBufferSize(64 * 1024);
    connect(m_reply, &QNetworkReply::redirected, this, [this](const QUrl &target) {
        if (!safeRedirect(target, m_state == State::Checking))
            fail("更新下载被重定向到不可信的地址。");
        else if (m_reply)
            m_reply->redirectAllowed();
    });
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateManager::consumeData);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateManager::finishRequest);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (m_state == State::Downloading)
            emit downloadProgress(received, total);
    });
    const QPointer<QNetworkReply> reply = m_reply;
    QTimer::singleShot(10 * 60 * 1000, this, [this, reply] {
        if (reply && reply == m_reply)
            fail("更新请求超时，请稍后重试。");
    });
}

void UpdateManager::consumeData()
{
    while (m_reply && m_reply->bytesAvailable() > 0)
    {
        const auto bytes = m_reply->read(64 * 1024);
        if (bytes.isEmpty())
            break;
        if (m_state == State::Downloading)
        {
            if (m_file.size() + bytes.size() > 512LL * 1024 * 1024)
            {
                fail("更新安装包超出大小限制。");
                return;
            }
            if (m_file.write(bytes) != bytes.size())
            {
                fail("安装包写入失败，请检查磁盘空间。");
                return;
            }
            m_hash.addData(bytes);
        }
        else
        {
            m_response.append(bytes);
            const qsizetype limit = m_state == State::Checking ? 1024 * 1024 : 4096;
            if (m_response.size() > limit)
            {
                fail("更新服务器响应过大。");
                return;
            }
        }
    }
}

void UpdateManager::finishRequest()
{
    consumeData();
    if (!m_reply)
        return;
    auto *reply = m_reply.data();
    m_reply = nullptr;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_state == State::Checking && status == 404)
    {
        fail("尚未找到公开的正式发布版本。");
        return;
    }
    if (m_state == State::Checking && !m_checkingReleasePage && status == 403
        && reply->rawHeader("X-RateLimit-Remaining").trimmed() == "0")
    {
        m_checkingReleasePage = true;
        request(latestReleasePage);
        return;
    }
    if (m_state == State::Checking && m_checkingReleasePage)
    {
        if (reply->error() != QNetworkReply::NoError || status != 302)
        {
            fail(QString("更新请求失败（HTTP %1）：%2").arg(status).arg(reply->errorString()));
            return;
        }
        const QUrl target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QString path = target.path(QUrl::FullyEncoded);
        if (target.scheme() != "https" || target.host() != "github.com"
            || !target.userInfo().isEmpty() || target.port() != -1
            || !target.query().isEmpty() || !target.fragment().isEmpty()
            || !path.startsWith(releaseTagPath) || !validTag(path.mid(releaseTagPath.size())))
        {
            fail("最新版本页面返回了无效的跳转地址。");
            return;
        }
        ReleaseInfo release;
        release.tagName = path.mid(releaseTagPath.size());
        release.version = QVersionNumber::fromString(release.tagName.mid(1));
        const bool newerVersion = release.version > QVersionNumber::fromString(APP_VERSION);
        if (newerVersion)
        {
            release.releaseName = "Release " + release.tagName;
            release.installerFileName = "SeismicWaveforms-Setup-" + release.tagName + ".exe";
            release.installerUrl = QUrl(releaseBase + release.tagName + "/" + release.installerFileName);
            release.checksumUrl = QUrl(release.installerUrl.toString() + ".sha256");
        }
        cancel();
        if (newerVersion)
            emit updateAvailable(release);
        else
            emit upToDate();
        return;
    }
    if (reply->error() != QNetworkReply::NoError || status != 200)
    {
        fail(QString("更新请求失败（HTTP %1）：%2").arg(status).arg(reply->errorString()));
        return;
    }
    if (m_state == State::Checking)
    {
        QString error;
        const auto release = parseRelease(m_response, QVersionNumber::fromString(APP_VERSION), &error);
        if (!error.isEmpty())
        {
            fail(error);
            return;
        }
        cancel();
        if (release)
            emit updateAvailable(*release);
        else
            emit upToDate();
    }
    else if (m_state == State::Checksum)
    {
        QString error;
        m_expectedHash = parseSha256(m_response, &error);
        if (m_expectedHash.isEmpty())
        {
            fail(error);
            return;
        }
        m_file.setFileName(m_directory->filePath(m_release.installerFileName + ".part"));
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        {
            fail("无法写入更新安装包。");
            return;
        }
        m_hash.reset();
        m_state = State::Downloading;
        request(m_release.installerUrl);
    }
    else if (m_state == State::Downloading)
    {
        if (!m_file.flush() || m_hash.result().toHex() != m_expectedHash)
        {
            fail("安装包 SHA-256 校验失败，已丢弃下载文件。请重试。");
            return;
        }
        m_file.close();
        const QString path = m_directory->filePath(m_release.installerFileName);
        if (!m_file.rename(path))
        {
            fail("无法保存已验证的安装包。");
            return;
        }
        m_state = State::Ready;
        emit readyToInstall(path);
    }
}

std::optional<ReleaseInfo> UpdateManager::parseRelease(const QByteArray &json,
                                                      const QVersionNumber &currentVersion,
                                                      QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    const auto reject = [errorMessage](const QString &message) -> std::optional<ReleaseInfo> {
        if (errorMessage)
            *errorMessage = message;
        return std::nullopt;
    };
    const auto document = QJsonDocument::fromJson(json);
    if (!document.isObject())
        return reject("版本信息格式不正确。");
    const auto object = document.object();
    ReleaseInfo release;
    release.tagName = object.value("tag_name").toString();
    if (!validTag(release.tagName))
        return reject("版本号必须为 vX.Y.Z 格式。");
    for (const QString &field : {"draft", "prerelease"})
    {
        if (!object.value(field).isBool() || object.value(field).toBool())
            return reject("该版本不是正式发布版本。");
    }
    release.version = QVersionNumber::fromString(release.tagName.mid(1));
    if (release.version <= currentVersion)
        return std::nullopt;
    release.releaseName = object.value("name").toString();
    release.notes = object.value("body").toString();
    release.installerFileName = "SeismicWaveforms-Setup-" + release.tagName + ".exe";
    const QString checksumName = release.installerFileName + ".sha256";
    int installers = 0;
    int checksums = 0;
    for (const auto &value : object.value("assets").toArray())
    {
        const auto asset = value.toObject();
        const QString name = asset.value("name").toString();
        if (name != release.installerFileName && name != checksumName)
            continue;
        const QString url = asset.value("browser_download_url").toString();
        if (url != releaseBase + release.tagName + "/" + name)
            return reject("更新附件地址不属于本项目的 HTTPS Release。");
        if (name == release.installerFileName)
        {
            release.installerUrl = QUrl(url);
            ++installers;
        }
        else
        {
            release.checksumUrl = QUrl(url);
            ++checksums;
        }
    }
    if (installers != 1 || checksums != 1)
        return reject("缺少唯一的 Windows x64 安装包或校验文件。");
    return release;
}

QByteArray UpdateManager::parseSha256(const QByteArray &contents, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    static const QRegularExpression pattern("^([0-9a-fA-F]{64})(?:[ \\t]+\\*?[^\\r\\n]+)?$");
    const auto match = pattern.match(QString::fromLatin1(contents.trimmed()));
    if (!match.hasMatch())
    {
        if (errorMessage)
            *errorMessage = "SHA-256 校验文件格式不正确。";
        return {};
    }
    return match.captured(1).toLatin1().toLower();
}

bool UpdateManager::verifySha256(const QString &filePath, const QByteArray &expectedHex)
{
    if (expectedHex.size() != 64 || parseSha256(expectedHex).isEmpty())
        return false;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == expectedHex.toLower();
}
