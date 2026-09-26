#include "updatemanager.h"
#include "fakenetwork.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTest>

namespace {
QJsonObject releaseJson(const QString &tag = "v1.2.0")
{
    const QString name = "SeismicWaveforms-Setup-" + tag + ".exe";
    const QString base = "https://github.com/Naporing/microseismic-monitor-qt/releases/download/" + tag + "/";
    return {{"tag_name", tag}, {"name", "Release " + tag}, {"body", "更新说明"},
            {"draft", false}, {"prerelease", false},
            {"assets", QJsonArray{QJsonObject{{"name", name}, {"browser_download_url", base + name}},
                                  QJsonObject{{"name", name + ".sha256"}, {"browser_download_url", base + name + ".sha256"}}}}};
}

std::optional<ReleaseInfo> parse(const QJsonObject &json, QString *error = nullptr)
{
    return UpdateManager::parseRelease(QJsonDocument(json).toJson(), QVersionNumber(1, 0, 0), error);
}
}

class UpdateManagerTest : public QObject
{
    Q_OBJECT
private slots:
    void checksAsynchronouslyAndRejectsDuplicateRequests()
    {
        FakeNetwork network;
        network.responses.enqueue({QJsonDocument(releaseJson()).toJson()});
        UpdateManager manager(nullptr, &network);
        QSignalSpy available(&manager, &UpdateManager::updateAvailable);
        manager.checkForUpdates();
        QVERIFY(manager.isBusy());
        manager.checkForUpdates();
        QCOMPARE(network.requests.size(), 1);
        QTRY_COMPARE(available.size(), 1);
        QVERIFY(!manager.isBusy());
        QCOMPARE(qvariant_cast<ReleaseInfo>(available[0][0]).tagName, QString("v1.2.0"));
        const auto request = network.requests.first();
        QCOMPARE(request.url().scheme(), QString("https"));
        QVERIFY(request.transferTimeout() > 0);
        QVERIFY(!request.rawHeader("User-Agent").isEmpty());
        QCOMPARE(request.rawHeader("Accept"), QByteArray("application/vnd.github+json"));
    }

    void reportsCurrentVersionAndFailures()
    {
        FakeNetwork network;
        network.responses.enqueue({QJsonDocument(releaseJson("v1.0.0")).toJson()});
        network.responses.enqueue({"invalid"});
        network.responses.enqueue({{}, 503, QNetworkReply::ServiceUnavailableError});
        network.responses.enqueue({{}, 404, QNetworkReply::ContentNotFoundError});
        UpdateManager manager(nullptr, &network);
        QSignalSpy current(&manager, &UpdateManager::upToDate);
        QSignalSpy failures(&manager, &UpdateManager::failed);
        manager.checkForUpdates();
        QTRY_COMPARE(current.size(), 1);
        manager.checkForUpdates();
        QTRY_COMPARE(failures.size(), 1);
        QCOMPARE(failures[0][1].toBool(), false);
        manager.checkForUpdates(true);
        QTRY_COMPARE(failures.size(), 2);
        QCOMPARE(failures[1][1].toBool(), true);
        manager.checkForUpdates();
        QTRY_COMPARE(failures.size(), 3);
        QVERIFY(failures[2][0].toString().contains("尚未"));
        QVERIFY(!manager.isBusy());
    }

    void checksReleasePageWhenApiQuotaIsExhausted()
    {
        FakeNetwork network;
        network.responses.enqueue({{}, 403, QNetworkReply::ContentAccessDenied, {}, "0"});
        network.responses.enqueue({{}, 302, QNetworkReply::NoError,
                                   QUrl("https://github.com/Naporing/microseismic-monitor-qt/releases/tag/v1.2.0")});
        UpdateManager manager(nullptr, &network);
        QSignalSpy available(&manager, &UpdateManager::updateAvailable);
        QSignalSpy failures(&manager, &UpdateManager::failed);
        manager.checkForUpdates();
        QTRY_COMPARE(network.requests.size(), 2);
        QTRY_COMPARE(available.size(), 1);
        QCOMPARE(failures.size(), 0);
        QCOMPARE(network.operations[1], QNetworkAccessManager::HeadOperation);
        QCOMPARE(network.requests[1].url(), QUrl("https://github.com/Naporing/microseismic-monitor-qt/releases/latest"));
        QCOMPARE(network.requests[1].attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 int(QNetworkRequest::ManualRedirectPolicy));
        const auto release = qvariant_cast<ReleaseInfo>(available[0][0]);
        QCOMPARE(release.tagName, QString("v1.2.0"));
        QCOMPARE(release.installerUrl, QUrl("https://github.com/Naporing/microseismic-monitor-qt/releases/download/v1.2.0/SeismicWaveforms-Setup-v1.2.0.exe"));
        QCOMPARE(release.checksumUrl, QUrl(release.installerUrl.toString() + ".sha256"));
    }

    void rejectsUntrustedReleasePageRedirect()
    {
        FakeNetwork network;
        network.responses.enqueue({{}, 403, QNetworkReply::ContentAccessDenied, {}, "0"});
        network.responses.enqueue({{}, 302, QNetworkReply::NoError, QUrl("https://example.com/releases/tag/v1.2.0")});
        UpdateManager manager(nullptr, &network);
        QSignalSpy available(&manager, &UpdateManager::updateAvailable);
        QSignalSpy failures(&manager, &UpdateManager::failed);
        manager.checkForUpdates();
        QTRY_COMPARE(failures.size(), 1);
        QCOMPARE(network.requests.size(), 2);
        QCOMPARE(available.size(), 0);
    }

    void releasePageCanReportCurrentVersion()
    {
        FakeNetwork network;
        network.responses.enqueue({{}, 403, QNetworkReply::ContentAccessDenied, {}, "0"});
        network.responses.enqueue({{}, 302, QNetworkReply::NoError,
                                   QUrl("https://github.com/Naporing/microseismic-monitor-qt/releases/tag/v1.0.1")});
        UpdateManager manager(nullptr, &network);
        QSignalSpy current(&manager, &UpdateManager::upToDate);
        manager.checkForUpdates();
        QTRY_COMPARE(current.size(), 1);
        QCOMPARE(network.requests.size(), 2);
    }

    void doesNotFallBackForOtherForbiddenResponses()
    {
        FakeNetwork network;
        network.responses.enqueue({{}, 403, QNetworkReply::ContentAccessDenied});
        UpdateManager manager(nullptr, &network);
        QSignalSpy failures(&manager, &UpdateManager::failed);
        manager.checkForUpdates();
        QTRY_COMPARE(failures.size(), 1);
        QCOMPARE(network.requests.size(), 1);
    }

    void downloadsVerifiesAndCancels()
    {
        const QByteArray bytes(100000, 'x');
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
        FakeNetwork network;
        network.responses.enqueue({hash + "  SeismicWaveforms-Setup-v1.2.0.exe"});
        network.responses.enqueue({bytes});
        QTemporaryDir directory;
        UpdateManager manager(nullptr, &network, directory.path());
        QSignalSpy ready(&manager, &UpdateManager::readyToInstall);
        QSignalSpy progress(&manager, &UpdateManager::downloadProgress);
        manager.downloadUpdate(*parse(releaseJson()));
        manager.checkForUpdates();
        QTRY_COMPARE(ready.size(), 1);
        QCOMPARE(network.requests.size(), 2);
        QVERIFY(!progress.isEmpty());
        const QString path = ready[0][0].toString();
        QVERIFY(UpdateManager::verifySha256(path, hash));
        manager.cancel();
        QVERIFY(!QFile::exists(path));
        QVERIFY(!manager.isBusy());
    }

    void neverOffersUnverifiedOrPartialFiles()
    {
        const auto release = *parse(releaseJson());
        for (bool truncated : {false, true})
        {
            FakeNetwork network;
            network.responses.enqueue({QByteArray(64, '0')});
            network.responses.enqueue({"corrupt", 200, truncated ? QNetworkReply::RemoteHostClosedError : QNetworkReply::NoError});
            QTemporaryDir directory;
            UpdateManager manager(nullptr, &network, directory.path());
            QSignalSpy ready(&manager, &UpdateManager::readyToInstall);
            QSignalSpy failures(&manager, &UpdateManager::failed);
            manager.downloadUpdate(release);
            QTRY_COMPARE(failures.size(), 1);
            QCOMPARE(ready.size(), 0);
            QCOMPARE(QDir(directory.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
        }
    }

    void rejectsUnsafeRedirectsAndInvalidDownloads()
    {
        FakeNetwork network;
        network.responses.enqueue({{}, 302, QNetworkReply::NoError, QUrl("https://example.com/install.exe")});
        UpdateManager manager(nullptr, &network);
        QSignalSpy failures(&manager, &UpdateManager::failed);
        manager.checkForUpdates();
        QTRY_COMPARE(failures.size(), 1);
        auto release = *parse(releaseJson());
        release.installerFileName = "../other.exe";
        manager.downloadUpdate(release);
        QCOMPARE(failures.size(), 2);
        QCOMPARE(network.requests.size(), 1);
    }

    void cancellationDoesNotReportAnError()
    {
        FakeNetwork network;
        network.responses.enqueue({QJsonDocument(releaseJson()).toJson()});
        UpdateManager manager(nullptr, &network);
        QSignalSpy failures(&manager, &UpdateManager::failed);
        QSignalSpy available(&manager, &UpdateManager::updateAvailable);
        manager.checkForUpdates();
        manager.cancel();
        QTest::qWait(10);
        QCOMPARE(failures.size(), 0);
        QCOMPARE(available.size(), 0);
        QVERIFY(!manager.isBusy());
    }

    void rejectsInvalidChecksumAndUnwritableDirectory()
    {
        FakeNetwork network;
        network.responses.enqueue({"invalid digest"});
        QTemporaryDir directory;
        UpdateManager manager(nullptr, &network, directory.path());
        QSignalSpy failures(&manager, &UpdateManager::failed);
        manager.downloadUpdate(*parse(releaseJson()));
        QTRY_COMPARE(failures.size(), 1);
        QCOMPARE(network.requests.size(), 1);
        QFile blockingFile(directory.filePath("file-instead-of-directory"));
        QVERIFY(blockingFile.open(QIODevice::WriteOnly));
        blockingFile.close();
        UpdateManager blocked(nullptr, &network, blockingFile.fileName());
        QSignalSpy blockedFailure(&blocked, &UpdateManager::failed);
        blocked.downloadUpdate(*parse(releaseJson()));
        QCOMPARE(blockedFailure.size(), 1);
        QCOMPARE(network.requests.size(), 1);
    }

    void preservesInstallerOnlyAfterHandoff()
    {
        const QByteArray bytes("installer");
        FakeNetwork network;
        network.responses.enqueue({QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()});
        network.responses.enqueue({bytes});
        QTemporaryDir directory;
        QString path;
        {
            UpdateManager manager(nullptr, &network, directory.path());
            QSignalSpy ready(&manager, &UpdateManager::readyToInstall);
            manager.downloadUpdate(*parse(releaseJson()));
            QTRY_COMPARE(ready.size(), 1);
            path = ready[0][0].toString();
            manager.preserveInstaller();
        }
        QVERIFY(QFile::exists(path));
    }

    void comparesNumericVersions()
    {
        const auto release = parse(releaseJson("v1.10.0"));
        QVERIFY(release);
        QCOMPARE(release->version, QVersionNumber(1, 10, 0));
        QCOMPARE(release->notes, QString("更新说明"));
        for (const QString &tag : {"v1.0.0", "v0.9.9"})
        {
            QString error = "old error";
            QVERIFY(!parse(releaseJson(tag), &error));
            QVERIFY(error.isEmpty());
        }
    }

    void rejectsInvalidVersions_data()
    {
        QTest::addColumn<QString>("tag");
        for (const QString &tag : {"", "1.2.0", "vv1.2.0", "v1.2", "v1.2.3.4", "v01.2.3", "v1.2.3-beta", "v1.2.3x", "v9999999999999.0.0"})
            QTest::newRow(qPrintable(tag)) << tag;
    }

    void rejectsInvalidVersions()
    {
        QFETCH(QString, tag);
        QString error;
        QVERIFY(!parse(releaseJson(tag), &error));
        QVERIFY(!error.isEmpty());
    }

    void rejectsMalformedAndUnstableReleases()
    {
        QString error;
        QVERIFY(!UpdateManager::parseRelease("[]", QVersionNumber(1, 0, 0), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!UpdateManager::parseRelease("broken", QVersionNumber(1, 0, 0), &error));
        for (const QString &field : {"draft", "prerelease"})
        {
            auto json = releaseJson();
            json[field] = true;
            QVERIFY(!parse(json, &error));
            QVERIFY(!error.isEmpty());
            json.remove(field);
            QVERIFY(!parse(json, &error));
        }
    }

    void rejectsUnsafeOrMissingAssets()
    {
        QString error;
        auto json = releaseJson();
        json["assets"] = QJsonArray{};
        QVERIFY(!parse(json, &error));
        QVERIFY(!error.isEmpty());
        for (const QString &url : {"http://github.com/file.exe", "https://example.com/file.exe",
                                   "https://github.com/other/project/releases/download/v1.2.0/file.exe",
                                   "file:///tmp/file.exe"})
        {
            json = releaseJson();
            auto assets = json["assets"].toArray();
            auto asset = assets[0].toObject();
            asset["browser_download_url"] = url;
            assets[0] = asset;
            json["assets"] = assets;
            QVERIFY(!parse(json, &error));
            QVERIFY(!error.isEmpty());
        }
        json = releaseJson();
        auto assets = json["assets"].toArray();
        assets.append(assets[0]);
        json["assets"] = assets;
        QVERIFY(!parse(json, &error));
    }

    void parsesAndVerifiesChecksum()
    {
        const QByteArray bytes("installer bytes");
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
        QCOMPARE(UpdateManager::parseSha256(hash + "  installer.exe\r\n"), hash);
        QCOMPARE(UpdateManager::parseSha256(hash.toUpper()), hash);
        for (const QByteArray &invalid : {QByteArray(), QByteArray("deadbeef"), hash + "garbage", hash + "\n" + hash})
        {
            QString error;
            QVERIFY(UpdateManager::parseSha256(invalid, &error).isEmpty());
            QVERIFY(!error.isEmpty());
        }
        QTemporaryDir directory;
        QFile file(directory.filePath("installer.exe"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        QVERIFY(UpdateManager::verifySha256(file.fileName(), hash));
        QVERIFY(file.open(QIODevice::Append));
        file.write("changed");
        file.close();
        QVERIFY(!UpdateManager::verifySha256(file.fileName(), hash));
        QVERIFY(!UpdateManager::verifySha256(directory.filePath("absent"), hash));
    }
};

QTEST_GUILESS_MAIN(UpdateManagerTest)
#include "test_updatemanager.moc"
