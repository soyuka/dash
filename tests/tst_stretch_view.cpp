#include <cmath>
#include <memory>

#include <QComboBox>
#include <QCommandLineParser>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPointingDevice>
#include <QQuickWidget>
#include <QSettings>
#include <QStandardPaths>
#include <QtTest>

#include "app/arbiter.hpp"
#include "app/launch_options.hpp"
#include "app/pages/openauto.hpp"
#include "app/stretch_view.hpp"
#include "app/widgets/dialog.hpp"
#include "app/window.hpp"

namespace {
const QSize logical(432, 240);
const QSize physical(2008, 240);
const double sx = 2008.0 / 432.0;

QPoint to_physical(QPointF point)
{
    return QPoint(std::lround(point.x() * sx), std::lround(point.y()));
}

class PressRecorder : public QObject {
   public:
    QList<QPointF> mouse;
    QList<QPointF> touch;
    QList<int> keys;

    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress)
            this->mouse.append(static_cast<QMouseEvent *>(event)->position());
        else if (event->type() == QEvent::KeyPress)
            this->keys.append(static_cast<QKeyEvent *>(event)->key());
        else if (event->type() == QEvent::TouchBegin)
            this->touch.append(static_cast<QTouchEvent *>(event)->points().first().position());
        return false;
    }
};
}

// One window for the whole run: openauto keeps its wireless listener bound
// for the life of the process, so a second window cannot start Android Auto.
class StretchViewTest : public QObject {
    Q_OBJECT

    MainWindow *window = nullptr;
    std::unique_ptr<StretchView> view;

    void reset_size()
    {
        this->view->resize(physical);
        QTRY_COMPARE(this->view->viewport()->size(), physical);
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
    }

    void lays_out_the_window_at_the_logical_size()
    {
        QCOMPARE(this->view->viewport()->size(), physical);
        QCOMPARE(this->window->size(), logical);
        QCOMPARE(this->window->graphicsProxyWidget(), this->view->proxy());
        QCOMPARE(this->view->sceneRect(), QRectF(QPointF(0, 0), logical));
    }

    void scales_the_view_to_fill_the_window()
    {
        QCOMPARE(this->view->transform(), QTransform::fromScale(sx, 1.0));
        QCOMPARE(this->view->mapFromScene(QPointF(logical.width(), logical.height())), QPoint(2008, 240));
    }

    void recomputes_the_scale_on_resize()
    {
        this->view->resize(864, 480);
        QTRY_COMPARE(this->view->transform(), QTransform::fromScale(2.0, 2.0));
        QCOMPARE(this->window->size(), logical);
        this->reset_size();
        QCOMPARE(this->view->transform(), QTransform::fromScale(sx, 1.0));
    }

    void routes_a_physical_click_to_the_logical_widget()
    {
        auto &arbiter = this->window->arbiter();
        auto settings = arbiter.layout().page(4);
        arbiter.set_curr_page(arbiter.layout().page(1));
        QCoreApplication::processEvents();

        auto button = settings->button();
        QVERIFY(button->isVisible());
        PressRecorder recorder;
        button->installEventFilter(&recorder);
        QPointF local(button->rect().center());
        QTest::mouseClick(this->view->viewport(), Qt::LeftButton, {}, to_physical(button->mapTo(this->window, local)));
        QTRY_COMPARE(arbiter.layout().curr_page, settings);
        QCOMPARE(recorder.mouse.size(), 1);
        QVERIFY(std::abs(recorder.mouse.first().x() - local.x()) <= 1.0);
        QVERIFY(std::abs(recorder.mouse.first().y() - local.y()) <= 1.0);
        button->removeEventFilter(&recorder);
    }

    void opens_dialogs_inside_the_scene()
    {
        auto dialogs = this->window->findChildren<Dialog *>();
        QVERIFY(!dialogs.isEmpty());
        for (auto dialog : dialogs) {
            dialog->open();
            QCoreApplication::processEvents();
            QVERIFY(!dialog->isWindow());
            QCOMPARE(dialog->window(), this->window);
            QVERIFY(dialog->isVisible());
            QVERIFY(QRect(QPoint(0, 0), logical).contains(dialog->geometry()));
            dialog->close();
        }
    }

    void embeds_popups_in_the_scene()
    {
        auto combo = new QComboBox(this->window->centralWidget());
        combo->addItems({"one", "two"});
        combo->show();
        combo->showPopup();
        auto popup = combo->view()->window();
        QTRY_VERIFY(popup->isVisible());
        auto proxy = popup->graphicsProxyWidget();
        QVERIFY(proxy);
        QCOMPARE(proxy->scene(), this->view->scene());
        combo->hidePopup();
        delete combo;
    }

    void maps_android_auto_input_to_the_logical_frame()
    {
        auto &arbiter = this->window->arbiter();
        arbiter.set_curr_page(arbiter.layout().page(0));
        auto frame = arbiter.layout().page(0)->container()->findChild<OpenAutoFrame *>();
        QVERIFY(frame);
        emit frame->toggle(true);
        QTRY_VERIFY(frame->isVisible());

        PressRecorder recorder;
        frame->installEventFilter(&recorder);
        QPointF local(frame->width() * 0.75, frame->height() * 0.5);
        auto physical_point = to_physical(frame->mapTo(this->window, local));

        QTest::mouseClick(this->view->viewport(), Qt::LeftButton, {}, physical_point);
        QTRY_COMPARE(recorder.mouse.size(), 1);
        QVERIFY(std::abs(recorder.mouse.first().x() - local.x()) <= 1.0);
        QVERIFY(std::abs(recorder.mouse.first().y() - local.y()) <= 1.0);

        auto device = QTest::createTouchDevice();
        QTest::touchEvent(this->view.get(), device).press(0, physical_point);
        QTest::touchEvent(this->view.get(), device).release(0, physical_point);
        QTRY_COMPARE(recorder.touch.size(), 1);
        QVERIFY(std::abs(recorder.touch.first().x() - local.x()) <= 1.0);
        QVERIFY(std::abs(recorder.touch.first().y() - local.y()) <= 1.0);

        this->view->activateWindow();
        frame->setFocus();
        QTest::keyClick(this->view.get(), Qt::Key_Return);
        QTRY_COMPARE(recorder.keys, QList<int>({Qt::Key_Return}));
        frame->removeEventFilter(&recorder);
    }

    void draws_the_android_auto_quick_widget_through_the_proxy()
    {
        auto frame = this->window->arbiter().layout().page(0)->container()->findChild<OpenAutoFrame *>();
        QVERIFY(frame && frame->isVisible());
        auto video = frame->findChild<QQuickWidget *>();
        QVERIFY(video);
        auto center = to_physical(video->mapTo(this->window, QPointF(video->rect().center())));

        // VideoOutput needs an RHI backend, so offscreen only its clear color shows.
        video->setClearColor(Qt::red);
        QTRY_COMPARE(this->view->viewport()->grab().toImage().pixelColor(center), QColor(Qt::red));
        video->setClearColor(Qt::blue);
        QTRY_COMPARE(this->view->viewport()->grab().toImage().pixelColor(center), QColor(Qt::blue));
        video->setClearColor(Qt::black);
        emit frame->toggle(false);
    }

    void minimizes_the_view_rather_than_the_embedded_window()
    {
        QPushButton *minimize = nullptr;
        for (auto button : this->window->findChildren<QPushButton *>()) {
            if (button->text() == "minimize")
                minimize = button;
        }
        QVERIFY(minimize);
        minimize->click();
        QVERIFY(this->view->isMinimized());
        QVERIFY(!this->window->isMinimized());
        this->view->showNormal();
        this->reset_size();
    }

    void cleanupTestCase()
    {
        this->view.reset();
    }
};

class LaunchOptionsTest : public QObject {
    Q_OBJECT

    static LaunchOptions resolve(const QStringList &arguments, const QVariant &configured)
    {
        QSettings settings;
        settings.clear();
        if (configured.isValid())
            settings.setValue("Window/logical_size", configured);

        QCommandLineParser parser;
        LaunchOptions options;
        options.add_to(parser);
        parser.parse(QStringList("dash") + arguments);
        options.resolve(parser, settings);
        return options;
    }

   private Q_SLOTS:
    void stretch_is_off_by_default()
    {
        auto options = resolve({}, {});
        QVERIFY(options.error.isEmpty());
        QVERIFY(!options.logical_size.isValid());
    }

    void reads_the_logical_size_from_the_config()
    {
        QCOMPARE(resolve({}, "432x240").logical_size, logical);
        QCOMPARE(resolve({}, logical).logical_size, logical);
    }

    void command_line_overrides_the_config()
    {
        QCOMPARE(resolve({"--logical-size", "500x200"}, "432x240").logical_size, QSize(500, 200));
    }

    void rejects_a_malformed_size()
    {
        auto options = resolve({"--logical-size", "432"}, {});
        QVERIFY(!options.error.isEmpty());
        QVERIFY(!resolve({}, "0x240").error.isEmpty());
    }

    void keeps_the_windowed_option()
    {
        QCOMPARE(resolve({"--windowed", "800x480"}, {}).window_size, QSize(800, 480));
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("openDsh");
    QCoreApplication::setApplicationName("dash-test");
    int status = 0;
    {
        LaunchOptionsTest options;
        status |= QTest::qExec(&options, argc, argv);
    }
    {
        StretchViewTest stretch;
        status |= QTest::qExec(&stretch, argc, argv);
    }
    return status;
}

#include "tst_stretch_view.moc"
