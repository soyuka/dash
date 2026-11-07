#include <QSettings>
#include <QStandardPaths>
#include <QtTest>

#include "app/arbiter.hpp"
#include "app/window.hpp"

class MainWindowTest : public QObject {
    Q_OBJECT

   private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("openDsh");
        QCoreApplication::setApplicationName("dash-test");
        QSettings().clear();
    }

    void builds_and_shows_every_page()
    {
        auto window = std::make_unique<MainWindow>(QRect(0, 0, 800, 480));
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        auto &arbiter = window->arbiter();
        QStringList names;
        for (auto page : arbiter.layout().pages())
            names.append(page->name());
        QCOMPARE(names, QStringList({"Android Auto", "Media", "Vehicle", "Launcher", "Settings"}));

        for (auto page : arbiter.layout().pages()) {
            arbiter.set_curr_page(page);
            QCoreApplication::processEvents();
            QCOMPARE(arbiter.layout().curr_page, page);
            QVERIFY(page->container()->isVisible());
        }

        arbiter.set_fullscreen(true);
        QCoreApplication::processEvents();
        arbiter.set_fullscreen(false);
        QCoreApplication::processEvents();
        QVERIFY(arbiter.layout().curr_page->container()->isVisible());
    }
};

QTEST_MAIN(MainWindowTest)
#include "tst_main_window.moc"
