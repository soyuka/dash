#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QPainter>

#include "mocked.hpp"

namespace {
class DimOverlay : public QWidget {
   public:
    explicit DimOverlay(QWidget *window) : QWidget(window)
    {
        this->setAttribute(Qt::WA_TransparentForMouseEvents);
        this->setAttribute(Qt::WA_NoSystemBackground);
        this->setGeometry(window->rect());
        window->installEventFilter(this);
    }

    void set_alpha(int alpha)
    {
        this->alpha = alpha;
        this->setVisible(alpha > 0);
        this->raise();
        this->update();
    }

   protected:
    void paintEvent(QPaintEvent *) override { QPainter(this).fillRect(this->rect(), QColor(0, 0, 0, this->alpha)); }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == this->parentWidget()) {
            if (event->type() == QEvent::Resize)
                this->setGeometry(this->parentWidget()->rect());
            else if (event->type() == QEvent::ChildAdded && this->isVisible())
                this->raise();
        }
        return false;
    }

   private:
    int alpha = 0;
};

QMainWindow *main_window()
{
    for (auto widget : QApplication::topLevelWidgets()) {
        if (auto window = qobject_cast<QMainWindow *>(widget))
            return window;
    }
    return nullptr;
}
}

Mocked::Mocked() = default;

Mocked::~Mocked()
{
    delete this->overlay;
}

bool Mocked::supported()
{
    return true;
}

uint8_t Mocked::priority()
{
    return 1;
}

void Mocked::set(int brightness)
{
    if (!this->overlay) {
        auto window = main_window();
        if (!window)
            return;
        this->overlay = new DimOverlay(window);
    }
    // never fully black: keep a dimmed screen readable
    int alpha = std::clamp(255 - brightness, 0, 255) * 200 / 255;
    static_cast<DimOverlay *>(this->overlay.data())->set_alpha(alpha);
}
