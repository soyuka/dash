#include <memory>

#include <QSettings>
#include <QStandardPaths>
#include <QtTest>

#include "app/arbiter.hpp"
#include "app/widgets/dialog.hpp"
#include "app/window.hpp"

// One window for the whole run: openauto keeps its wireless listener bound
// for the life of the process, so a second window cannot start Android Auto.
class MainWindowTest : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> window;

   private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("openDsh");
        QCoreApplication::setApplicationName("dash-test");
        QSettings settings;
        settings.clear();
        settings.setValue("Pages/Vehicle/plugin", "test");

        this->window = std::make_unique<MainWindow>();
        this->window->resize(800, 480);
        this->window->show();
        QVERIFY(QTest::qWaitForWindowExposed(this->window.get()));
    }

    void builds_and_shows_every_page()
    {
        auto &arbiter = this->window->arbiter();
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
    }

    void loads_the_vehicle_plugin()
    {
        auto &arbiter = this->window->arbiter();
        auto vehicle = qobject_cast<QTabWidget *>(arbiter.layout().page(2)->container()->findChild<QTabWidget *>());
        QVERIFY(vehicle);
        QStringList tabs;
        for (int i = 0; i < vehicle->count(); i++)
            tabs.append(vehicle->tabText(i));
        QCOMPARE(tabs, QStringList({"Data", "Vehicle"}));
    }

    void toggles_fullscreen_pages()
    {
        auto &arbiter = this->window->arbiter();
        for (int toggler = 0; toggler < arbiter.layout().fullscreen.togglers().size(); toggler++) {
            arbiter.set_curr_fullscreen_toggler(toggler);
            arbiter.set_fullscreen(true);
            QCoreApplication::processEvents();
            auto widget = arbiter.layout().fullscreen.curr_toggler->widget();
            QVERIFY(!widget->isWindow());
            if (widget->isVisible())
                QVERIFY(this->window->rect().contains(widget->geometry()));
            arbiter.set_fullscreen(false);
            QCoreApplication::processEvents();
            QVERIFY(arbiter.layout().curr_page->container()->isVisible());
        }
    }

    void dialogs_open_inside_the_main_window()
    {
        auto dialogs = this->window->findChildren<Dialog *>();
        QVERIFY(dialogs.size() > 5);
        for (auto dialog : dialogs) {
            dialog->open();
            QCoreApplication::processEvents();
            QVERIFY(!dialog->isWindow());
            QVERIFY(dialog->isVisible());
            QVERIFY(this->window->rect().contains(dialog->geometry()));
            dialog->close();
            QVERIFY(!dialog->isVisible());
        }
    }

    void cleanupTestCase()
    {
        this->window.reset();
    }
};

QTEST_MAIN(MainWindowTest)
#include "tst_main_window.moc"
