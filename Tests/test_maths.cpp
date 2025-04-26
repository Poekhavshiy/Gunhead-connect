#include <QtTest>
#include <QObject>

class MathTests : public QObject {
    Q_OBJECT

private slots:
    void additionTest() {
        QCOMPARE(1 + 1, 2);
    }

    void subtractionTest() {
        QCOMPARE(5 - 3, 2);
    }
};

QTEST_MAIN(MathTests)
#include "test_math.moc"
