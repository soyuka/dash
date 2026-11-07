#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>

#include "app/arbiter.hpp"

#include "app/widgets/fullscreen_toggler.hpp"

FullscreenToggler::FullscreenToggler(Arbiter &arbiter, QString name, QWidget *widget)
    : arbiter(arbiter)
    , name_(name)
    , widget_(widget)
{
}

NullFullscreenToggler::NullFullscreenToggler(Arbiter &arbiter)
    : QFrame(arbiter.window())
    , FullscreenToggler(arbiter, "none", this)
{
    this->setVisible(false);
}

BarFullscreenToggler::BarFullscreenToggler(Arbiter &arbiter)
    : QFrame(arbiter.window())
    , FullscreenToggler(arbiter, "bar", this)
{
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto bar = new QPushButton();
    connect(bar, &QPushButton::clicked, [&arbiter]{ arbiter.set_fullscreen(false); });
    bar->setFixedHeight(10 * arbiter.layout().scale);
    bar->setObjectName("FullscreenBar");

    layout->addStretch(4);
    layout->addWidget(bar, 2);
    layout->addStretch(4);

    this->setVisible(false);
}

ButtonFullscreenToggler::ButtonFullscreenToggler(Arbiter &arbiter)
    : QFrame(arbiter.window())
    , FullscreenToggler(arbiter, "button", this)
    , label(new QLabel(" X "))
{
    this->setObjectName("FullscreenButton");
    this->setFocusPolicy(Qt::NoFocus);

    this->label->setMargin(2);
    this->label->setAlignment(Qt::AlignCenter);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(this->label);

    this->setVisible(false);
}

void ButtonFullscreenToggler::enable()
{
    this->adjustSize();
    this->move_within_window(this->pos());
    this->show();
    this->raise();
}

void ButtonFullscreenToggler::move_within_window(QPoint pos)
{
    auto window = this->parentWidget();
    pos.setX(std::clamp(pos.x(), 0, std::max(0, window->width() - this->width())));
    pos.setY(std::clamp(pos.y(), 0, std::max(0, window->height() - this->height())));
    this->move(pos);
}

void ButtonFullscreenToggler::mousePressEvent(QMouseEvent *event)
{
    this->grab = event->position().toPoint();
    this->touch.start();
}

void ButtonFullscreenToggler::mouseReleaseEvent(QMouseEvent *)
{
    if (this->touch.isValid() && !this->touch.hasExpired(150))
        this->arbiter.set_fullscreen(false);
}

void ButtonFullscreenToggler::mouseMoveEvent(QMouseEvent *event)
{
    this->move_within_window(this->mapToParent(event->position().toPoint()) - this->grab);
}
