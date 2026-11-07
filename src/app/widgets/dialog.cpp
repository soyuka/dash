#include <cmath>

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>

#include "app/arbiter.hpp"
#include "app/keypad_navigator.hpp"

#include "app/widgets/dialog.hpp"

Dialog::Dialog(Arbiter &arbiter, bool fullscreen, QWidget *anchor)
    : QWidget(arbiter.window())
    , arbiter(arbiter)
    , anchor(anchor)
    , fullscreen(fullscreen)
{
    this->setVisible(false);
    this->setFocusPolicy(Qt::StrongFocus);

    auto layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    this->frame = this->content_widget();
    if (this->fullscreen) {
        int margin = std::ceil(48 * this->arbiter.layout().scale);
        layout->setContentsMargins(margin, margin, margin, margin);
        auto column = new QVBoxLayout();
        column->addStretch();
        column->addWidget(this->frame);
        column->addStretch();
        layout->addStretch();
        layout->addLayout(column);
        layout->addStretch();
    }
    else {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(this->frame);
    }

    this->timer = new QTimer(this);
    this->timer->setSingleShot(true);
    connect(this->timer, &QTimer::timeout, this, &QWidget::close);

    this->parentWidget()->installEventFilter(this);
    this->installEventFilter(this);
}

Dialog *Dialog::top(QWidget *window)
{
    const auto children = window->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        auto dialog = qobject_cast<Dialog *>(*it);
        if (dialog && dialog->isVisible() && !qobject_cast<SnackBar *>(dialog))
            return dialog;
    }
    return nullptr;
}

void Dialog::open(int timeout)
{
    auto focus = this->window()->focusWidget();
    if (!this->isAncestorOf(focus))
        this->return_focus = focus;

    this->show();
    this->raise();
    if (!this->isAncestorOf(this->window()->focusWidget())) {
        if (auto first = keypad::first(this))
            first->setFocus(Qt::OtherFocusReason);
        else if (this->fullscreen)
            this->setFocus();
    }
    if (timeout > 0)
        this->timer->start(timeout);
}

void Dialog::set_title(QString str)
{
    QLabel *label = new QLabel(str, this);
    QFont font(this->arbiter.forge().font(16));
    font.setBold(true);
    label->setFont(font);
    this->title->addWidget(label);
}

void Dialog::set_body(QWidget *widget)
{
    if (this->fullscreen) {
        QScrollArea *scroll_area = new QScrollArea(this);
        Session::Forge::to_touch_scroller(scroll_area);
        scroll_area->setWidgetResizable(true);
        scroll_area->setWidget(widget);

        this->body->addWidget(scroll_area);
    }
    else {
        this->body->addWidget(widget);
    }
}

void Dialog::set_button(QPushButton *button)
{
    if (this->buttons->count() == 0)
        this->add_cancel_button();
    button->setFlat(true);
    this->buttons->addWidget(button, 0, Qt::AlignRight);
    connect(button, &QPushButton::clicked, this, &QWidget::close);
}

void Dialog::add_cancel_button()
{
    this->buttons->addStretch();

    QPushButton *button = new QPushButton("cancel", this);
    button->setFlat(true);
    connect(button, &QPushButton::clicked, this, &QWidget::close);
    this->buttons->addWidget(button, 0, Qt::AlignRight);
}

QFrame *Dialog::content_widget()
{
    QFrame *frame = new QFrame(this);
    frame->setObjectName("DialogFrame");
    QVBoxLayout *layout = new QVBoxLayout(frame);

    this->title = new QVBoxLayout();
    this->title->setContentsMargins(0, 0, 0, 0);
    this->title->setSpacing(0);
    layout->addLayout(this->title);

    this->body = new QVBoxLayout();
    this->body->setContentsMargins(0, 0, 0, 0);
    this->body->setSpacing(0);
    layout->addLayout(this->body);

    this->buttons = new QHBoxLayout();
    this->buttons->setContentsMargins(0, 0, 0, 0);
    this->buttons->setSpacing(0);
    if (this->fullscreen)
        this->add_cancel_button();
    layout->addLayout(this->buttons);

    return frame;
}

void Dialog::update_geometry()
{
    auto window = this->parentWidget();
    if (this->fullscreen) {
        this->setGeometry(window->rect());
        return;
    }

    this->adjustSize();
    auto size = this->sizeHint().boundedTo(window->size());
    this->resize(size);

    if (!this->anchor || !this->anchor->isVisible()) {
        this->move(QPoint((window->width() - size.width()) / 2, (window->height() - size.height()) / 2));
        return;
    }

    // open next to the anchor, towards the centre of the window
    QRect anchor_rect(this->anchor->mapTo(window, QPoint()), this->anchor->size());
    QPoint window_center = window->rect().center();
    int offset = std::ceil(4 * this->arbiter.layout().scale);

    int x = (anchor_rect.center().x() > window_center.x())
        ? anchor_rect.center().x() - size.width()
        : anchor_rect.center().x();
    int y = (anchor_rect.center().y() > window_center.y())
        ? anchor_rect.top() - offset - size.height()
        : anchor_rect.bottom() + offset;

    x = std::clamp(x, 0, std::max(0, window->width() - size.width()));
    y = std::clamp(y, 0, std::max(0, window->height() - size.height()));
    this->move(x, y);
}

void Dialog::showEvent(QShowEvent *event)
{
    this->update_geometry();
    QWidget::showEvent(event);
}

void Dialog::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);

    auto focus = this->window()->focusWidget();
    bool lost = !focus || focus == this || this->isAncestorOf(focus) || !focus->isVisible();
    if (lost && this->return_focus && this->return_focus->isVisible())
        this->return_focus->setFocus(Qt::OtherFocusReason);
}

void Dialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && this->fullscreen)
        this->close();
    else
        QWidget::keyPressEvent(event);
}

void Dialog::paintEvent(QPaintEvent *)
{
    if (this->fullscreen)
        QPainter(this).fillRect(this->rect(), QColor(0, 0, 0, 128));
}

bool Dialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == this->parentWidget()) {
        if (event->type() == QEvent::Resize && this->isVisible())
            this->update_geometry();
        return false;
    }

    // any interaction keeps a timed dialog open
    if (this->timer->isActive())
        this->timer->start(this->timer->interval());

    return QWidget::eventFilter(object, event);
}

SnackBar::SnackBar(Arbiter &arbiter)
    : Dialog(arbiter, false)
{
    this->setFixedHeight(64 * this->arbiter.layout().scale);
}

void SnackBar::showEvent(QShowEvent *event)
{
    Dialog::showEvent(event);

    // centred right above the control bar, where the MsgRef placeholder sits
    auto window = this->parentWidget();
    this->setFixedWidth(window->width() * (2 / 3.0));
    int bottom = window->height();
    if (auto ref = window->findChild<QWidget *>("MsgRef"); ref && ref->isVisible())
        bottom = ref->mapTo(window, QPoint()).y();
    this->move((window->width() - this->width()) / 2, std::max(0, bottom - this->height()));
}

void SnackBar::mousePressEvent(QMouseEvent *)
{
    this->close();
}
