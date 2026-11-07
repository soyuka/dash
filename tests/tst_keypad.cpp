#include <cmath>
#include <memory>

#include <QAbstractSlider>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

#include "app/action.hpp"
#include "app/arbiter.hpp"
#include "app/keypad_navigator.hpp"
#include "app/pages/openauto.hpp"
#include "app/stretch_view.hpp"
#include "app/widgets/dialog.hpp"
#include "app/window.hpp"

namespace {
const QSize logical(432, 240);
const QSize physical(2008, 240);

const QList<QPair<QString, QString>> keypad_table = {
    {"aa_left", "Left"},
    {"aa_right", "Right"},
    {"aa_up", "Up"},
    {"aa_down", "Down"},
    {"aa_enter", "Return"},
    {"aa_back", "Esc"},
    {"aa_scroll_up", "PgUp"},
    {"aa_scroll_down", "PgDown"},
    {"aa_phone", "F1"},
    {"aa_home", "F2"},
    {"aa_voice", "F3"},
    {"aa_toggle_play", "F4"},
    {"cycle_page", "F5"},
    {"toggle_dark_mode", "F6"},
    {"toggle_fullscreen", "F7"},
    {"show_page_settings", "F8"},
};

QStringList current_ids()
{
    auto ids = Session::Core::LEGACY_ACTION_IDS;
    ids.removeAll("show_page_camera");
    return ids;
}

QList<Action::ActionState> states(const QSignalSpy &spy)
{
    QList<Action::ActionState> result;
    for (const auto &args : spy)
        result.append(args.at(0).value<Action::ActionState>());
    return result;
}
}

class BindingTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir;

    std::unique_ptr<QSettings> settings(const QString &name)
    {
        return std::make_unique<QSettings>(this->dir.filePath(name), QSettings::IniFormat);
    }

   private Q_SLOTS:
    void migrates_upstream_indices_to_ids()
    {
        // the 1657 keypad config as written by upstream openDsh (camera page present)
        auto s = this->settings("upstream.conf");
        s->setValue("Core/Action/0", "Left");
        s->setValue("Core/Action/1", "Return");
        s->setValue("Core/Action/5", "Esc");
        s->setValue("Core/Action/15", "PgUp");
        s->setValue("Core/Action/16", "PgDown");
        s->setValue("Core/Action/17", "F6");
        s->setValue("Core/Action/22", "F7");
        s->setValue("Core/Action/26", "F9");
        s->setValue("Core/Action/27", "F10");
        s->setValue("Core/Action/28", "F8");
        s->setValue("Core/Action/29", "F5");

        auto bindings = Session::Core::load_bindings(*s, current_ids());

        QCOMPARE(bindings.value("aa_left"), QString("Left"));
        QCOMPARE(bindings.value("aa_enter"), QString("Return"));
        QCOMPARE(bindings.value("aa_back"), QString("Esc"));
        QCOMPARE(bindings.value("aa_scroll_up"), QString("PgUp"));
        QCOMPARE(bindings.value("aa_scroll_down"), QString("PgDown"));
        QCOMPARE(bindings.value("toggle_dark_mode"), QString("F6"));
        QCOMPARE(bindings.value("toggle_fullscreen"), QString("F7"));
        QCOMPARE(bindings.value("show_page_launcher"), QString("F10"));
        QCOMPARE(bindings.value("show_page_settings"), QString("F8"));
        QCOMPARE(bindings.value("cycle_page"), QString("F5"));
        QVERIFY(!bindings.contains("show_page_camera"));

        s->beginGroup("Core");
        QVERIFY(!s->childGroups().contains("Action"));
        s->endGroup();
        QCOMPARE(s->value("Core/Bindings/show_page_settings").toString(), QString("F8"));
        QCOMPARE(s->value("Core/Bindings/cycle_page").toString(), QString("F5"));
        QVERIFY(!s->contains("Core/Bindings/show_page_camera"));
    }

    void binds_the_keypad_by_default()
    {
        auto s = this->settings("empty.conf");
        auto bindings = Session::Core::load_bindings(*s, current_ids());
        for (const auto &[id, key] : keypad_table)
            QCOMPARE(bindings.value(id), key);
    }

    void stored_bindings_win_over_defaults()
    {
        auto s = this->settings("custom.conf");
        s->setValue("Core/Bindings/aa_enter", "");
        s->setValue("Core/Bindings/aa_home", "F5");

        auto bindings = Session::Core::load_bindings(*s, current_ids());

        QVERIFY(!bindings.contains("aa_enter"));
        QCOMPARE(bindings.value("aa_home"), QString("F5"));
        // F5 is taken, so the cycle_page default does not apply
        QVERIFY(!bindings.contains("cycle_page"));
        QCOMPARE(bindings.value("aa_back"), QString("Esc"));
    }

    void ignores_unknown_ids()
    {
        auto s = this->settings("unknown.conf");
        s->setValue("Core/Bindings/no_such_action", "F12");
        auto bindings = Session::Core::load_bindings(*s, {"cycle_page"});
        QCOMPARE(bindings.keys(), QStringList({"cycle_page"}));
    }
};

// One window for the whole run: openauto keeps its wireless listener bound
// for the life of the process, so a second window cannot start Android Auto.
class KeypadTest : public QObject {
    Q_OBJECT

    MainWindow *window = nullptr;
    std::unique_ptr<StretchView> view;

    Arbiter &arbiter() { return this->window->arbiter(); }
    Page *page(const QString &id)
    {
        for (auto page : this->arbiter().layout().pages()) {
            if (page->id() == id)
                return page;
        }
        return nullptr;
    }
    QWidget *focus() { return this->window->focusWidget(); }
    void press(int key)
    {
        QTest::keyClick(this->view.get(), Qt::Key(key));
        QCoreApplication::processEvents();
    }
    QWidget *dialog_open()
    {
        for (auto dialog : this->window->findChildren<Dialog *>()) {
            if (dialog->isVisible())
                return dialog;
        }
        return nullptr;
    }
    void focus_rail(const QString &id)
    {
        this->arbiter().set_curr_page(this->page(id));
        QCoreApplication::processEvents();
        this->page(id)->button()->setFocus();
    }

   private Q_SLOTS:
    void initTestCase()
    {
        QSettings settings;
        settings.clear();
        settings.setValue("Pages/Vehicle/plugin", "test");

        this->window = new MainWindow();
        this->view = std::make_unique<StretchView>(this->window, logical);
        this->view->resize(physical);
        this->view->show();
        QVERIFY(QTest::qWaitForWindowExposed(this->view.get()));
        this->view->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(this->view.get()));
    }

    void binds_actions_by_id()
    {
        auto &core = this->arbiter().core();
        for (const auto &[id, key] : keypad_table) {
            auto action = core.action(id);
            QVERIFY2(action, qPrintable(id));
            QCOMPARE(action->key(), key);
        }
        QVERIFY(!core.action("show_page_camera"));

        auto cycle = core.action("cycle_page");
        this->arbiter().set_action(cycle, "F9");
        QCOMPARE(this->arbiter().settings().value("Core/Bindings/cycle_page").toString(), QString("F9"));
        QVERIFY(!this->arbiter().settings().contains("Core/Action/28"));
        this->arbiter().set_action(cycle, QString());
        QCOMPARE(this->arbiter().settings().value("Core/Bindings/cycle_page").toString(), QString(""));
        this->arbiter().set_action(cycle, "F5");
    }

    void first_key_focuses_the_rail()
    {
        this->arbiter().set_curr_page(this->page("android_auto"));
        QCoreApplication::processEvents();
        this->window->setFocus();
        this->press(Qt::Key_Down);
        QCOMPARE(this->focus(), this->page("android_auto")->button());
    }

    void walks_the_rail_with_up_and_down()
    {
        this->focus_rail("android_auto");
        QList<QWidget *> visited;
        for (int i = 0; i < 5; i++) {
            this->press(Qt::Key_Down);
            visited.append(this->focus());
        }
        QCOMPARE(visited, QList<QWidget *>({
            this->page("media")->button(),
            this->page("vehicle")->button(),
            this->page("launcher")->button(),
            this->page("settings")->button(),
            this->page("android_auto")->button(),
        }));
        this->press(Qt::Key_Up);
        QCOMPARE(this->focus(), this->page("settings")->button());
        QCOMPARE(this->arbiter().layout().curr_page, this->page("android_auto"));
    }

    void return_opens_the_page_and_enters_it()
    {
        this->focus_rail("android_auto");
        this->press(Qt::Key_Up);
        QCOMPARE(this->focus(), this->page("settings")->button());
        this->press(Qt::Key_Return);
        auto settings = this->page("settings");
        QCOMPARE(this->arbiter().layout().curr_page, settings);
        QVERIFY(this->focus());
        QVERIFY(settings->container()->isAncestorOf(this->focus()));
        QVERIFY(qobject_cast<QTabBar *>(this->focus()));
    }

    void arrows_move_within_the_page()
    {
        auto tabs = this->page("settings")->container()->findChild<QTabWidget *>();
        QVERIFY(tabs);
        tabs->setCurrentIndex(0);
        tabs->tabBar()->setFocus();

        this->press(Qt::Key_Right);
        QCOMPARE(tabs->currentIndex(), 1);
        this->press(Qt::Key_Left);
        QCOMPARE(tabs->currentIndex(), 0);

        this->press(Qt::Key_Down);
        QVERIFY(tabs->currentWidget()->isAncestorOf(this->focus()));
        auto first = this->focus();
        this->press(Qt::Key_Down);
        QVERIFY(tabs->currentWidget()->isAncestorOf(this->focus()));
        QVERIFY(this->focus() != first);
        QVERIFY(this->focus()->mapTo(this->window, QPoint()).y() > first->mapTo(this->window, QPoint()).y());

        tabs->tabBar()->setFocus();
        this->press(Qt::Key_Left);
        QCOMPARE(this->focus(), this->page("settings")->button());
    }

    void escape_returns_to_the_rail()
    {
        auto tabs = this->page("settings")->container()->findChild<QTabWidget *>();
        tabs->tabBar()->setFocus();
        this->press(Qt::Key_Down);
        QVERIFY(this->page("settings")->container()->isAncestorOf(this->focus()));
        this->press(Qt::Key_Escape);
        QCOMPARE(this->focus(), this->page("settings")->button());
        QCOMPARE(this->arbiter().layout().curr_page, this->page("settings"));
    }

    void wheel_changes_the_focused_value()
    {
        auto tabs = this->page("settings")->container()->findChild<QTabWidget *>();
        tabs->setCurrentIndex(0);
        QAbstractSlider *volume = nullptr;
        for (auto slider : tabs->currentWidget()->findChildren<QAbstractSlider *>()) {
            if (slider->maximum() == 100)
                volume = slider;
        }
        QVERIFY(volume);
        this->arbiter().set_volume(50);
        volume->setFocus();

        this->press(Qt::Key_PageDown);
        QVERIFY(volume->value() > 50);
        QCOMPARE(int(this->arbiter().system().volume), volume->value());
        this->press(Qt::Key_PageUp);
        QCOMPARE(volume->value(), 50);
        QCOMPARE(int(this->arbiter().system().volume), 50);
    }

    void escape_closes_a_dialog()
    {
        this->focus_rail("android_auto");
        this->press(Qt::Key_Return);
        auto aa = this->page("android_auto");
        QCOMPARE(this->arbiter().layout().curr_page, aa);
        auto opener = qobject_cast<QPushButton *>(this->focus());
        QVERIFY(opener);
        QVERIFY(aa->container()->isAncestorOf(opener));

        this->press(Qt::Key_Return);
        auto dialog = this->dialog_open();
        QVERIFY(dialog);
        QVERIFY(dialog->isAncestorOf(this->focus()));

        this->press(Qt::Key_Down);
        QVERIFY(dialog->isAncestorOf(this->focus()));

        this->press(Qt::Key_Escape);
        QVERIFY(!dialog->isVisible());
        QCOMPARE(this->focus(), opener);
    }

    void shows_a_focus_ring_readable_once_stretched()
    {
        this->focus_rail("media");
        this->press(Qt::Key_Up);
        auto button = this->page("android_auto")->button();
        QCOMPARE(this->focus(), button);

        auto ring = this->window->findChild<FocusRing *>();
        QVERIFY(ring);
        QVERIFY(ring->isVisible());
        QCOMPARE(ring->parentWidget(), button);
        QCOMPARE(ring->geometry(), button->rect());
        QVERIFY(ring->thickness() >= 3);

        // the left stroke survives the squeeze back to 432 columns: >= 3 logical px
        auto image = this->view->viewport()->grab().toImage();
        auto origin = button->mapTo(this->window, QPoint(0, button->height() / 2));
        auto color = ring->color().rgb();
        const double sx = double(physical.width()) / logical.width();
        int x = std::lround(origin.x() * sx);
        int run = 0;
        while (x + run < image.width() && image.pixel(x + run, origin.y()) == color)
            run++;
        QVERIFY2(run >= std::floor(3 * sx), qPrintable(QString("stroke is %1 physical px").arg(run)));
    }

    void keys_reach_android_auto_when_it_is_in_front()
    {
        auto aa = this->page("android_auto");
        this->focus_rail("android_auto");
        auto frame = aa->container()->findChild<OpenAutoFrame *>();
        QVERIFY(frame);
        emit frame->toggle(true);
        QTRY_VERIFY(frame->isVisible());
        QVERIFY(this->arbiter().android_auto_in_front());

        auto &core = this->arbiter().core();
        QSignalSpy enter(core.action("aa_enter"), &Action::fired);
        QSignalSpy down(core.action("aa_down"), &Action::fired);
        QSignalSpy back(core.action("aa_back"), &Action::fired);
        auto before = this->focus();

        this->press(Qt::Key_Return);
        this->press(Qt::Key_Down);
        this->press(Qt::Key_Escape);

        QCOMPARE(states(enter), QList<Action::ActionState>({Action::Activated, Action::Deactivated}));
        QCOMPARE(states(down), QList<Action::ActionState>({Action::Activated, Action::Deactivated}));
        QCOMPARE(states(back), QList<Action::ActionState>({Action::Activated, Action::Deactivated}));
        QCOMPARE(this->focus(), before);
        QCOMPARE(this->arbiter().layout().curr_page, aa);
        QVERIFY(!this->window->findChild<FocusRing *>()->isVisible());

        this->press(Qt::Key_F5);
        QTRY_COMPARE(this->arbiter().layout().curr_page, this->page("media"));
        QVERIFY(!this->arbiter().android_auto_in_front());

        emit frame->toggle(false);
        QCoreApplication::processEvents();
    }

    void keys_stay_in_dash_when_android_auto_is_not_projecting()
    {
        this->focus_rail("android_auto");
        QVERIFY(!this->arbiter().android_auto_in_front());
        QSignalSpy enter(this->arbiter().core().action("aa_enter"), &Action::fired);
        this->press(Qt::Key_Return);
        QCOMPARE(enter.count(), 0);
        QVERIFY(this->page("android_auto")->container()->isAncestorOf(this->focus()));
    }

    void cleanupTestCase()
    {
        this->view.reset();
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("openDsh");
    QCoreApplication::setApplicationName("dash-test-keypad");
    int status = 0;
    {
        BindingTest bindings;
        status |= QTest::qExec(&bindings, argc, argv);
    }
    {
        KeypadTest keypad;
        status |= QTest::qExec(&keypad, argc, argv);
    }
    return status;
}

#include "tst_keypad.moc"
