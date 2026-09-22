#include "updatemanager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
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
