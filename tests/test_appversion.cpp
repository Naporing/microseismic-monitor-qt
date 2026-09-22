#include "appversion.h"

#include <QTest>
#include <QVersionNumber>

class AppVersionTest : public QObject
{
    Q_OBJECT
private slots:
    void exposesSemanticVersion()
    {
        const QString version = QStringLiteral(APP_VERSION);
        qsizetype suffix = 0;
        const auto parsed = QVersionNumber::fromString(version, &suffix);
        QCOMPARE(parsed.segmentCount(), 3);
        QCOMPARE(suffix, version.size());
        QVERIFY(parsed >= QVersionNumber(1, 0, 0));
    }
};

QTEST_GUILESS_MAIN(AppVersionTest)
#include "test_appversion.moc"
