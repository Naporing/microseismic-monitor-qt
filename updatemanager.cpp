#include "updatemanager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
const QString releaseBase = "https://github.com/Naporing/microseismic-monitor-qt/releases/download/";

bool validTag(const QString &tag)
{
    static const QRegularExpression pattern("^v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$");
    if (!pattern.match(tag).hasMatch())
        return false;
    qsizetype suffix = 0;
    const auto version = QVersionNumber::fromString(QStringView(tag).mid(1), &suffix);
    return suffix == tag.size() - 1 && version.segmentCount() == 3;
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
