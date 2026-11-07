#include <algorithm>
#include <cmath>
#include <limits>

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>

#include "app/action.hpp"
#include "app/arbiter.hpp"
#include "app/pages/openauto.hpp"
#include "app/widgets/dialog.hpp"

#include "app/keypad_navigator.hpp"

namespace {
bool is_stop(QWidget *widget)
{
    return widget->isVisible() && widget->isEnabled() && !widget->isWindow() && !widget->focusProxy() &&
        (widget->focusPolicy() & Qt::TabFocus) && !widget->size().isEmpty() &&
        !qobject_cast<QScrollBar *>(widget) && !qobject_cast<FocusRing *>(widget);
}

QRect rect_in(QWidget *widget, QWidget *window)
{
    return QRect(widget->mapTo(window, QPoint()), widget->size());
}

bool is_arrow(int key)
{
    return key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left || key == Qt::Key_Right;
}

bool is_navigation(int key)
{
    switch (key) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Escape:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            return true;
        default:
            return false;
    }
}

int current_index(QWidget *widget)
{
    if (auto view = qobject_cast<QAbstractItemView *>(widget))
        return view->currentIndex().row();
    if (auto bar = qobject_cast<QTabBar *>(widget))
        return bar->currentIndex();
    return -1;
}
}

QList<QWidget *> keypad::focusables(QWidget *root)
{
    auto window = root->window();
    QList<QWidget *> stops;
    for (auto widget : root->findChildren<QWidget *>()) {
        if (widget->window() == window && is_stop(widget))
            stops.append(widget);
    }

    QList<QWidget *> leaves;
    for (auto widget : stops) {
        if (std::none_of(stops.begin(), stops.end(), [widget](QWidget *other){ return other != widget && widget->isAncestorOf(other); }))
            leaves.append(widget);
    }

    std::stable_sort(leaves.begin(), leaves.end(), [window](QWidget *a, QWidget *b){
        auto ra = rect_in(a, window);
        auto rb = rect_in(b, window);
        return (ra.top() != rb.top()) ? ra.top() < rb.top() : ra.left() < rb.left();
    });
    return leaves;
}

QWidget *keypad::first(QWidget *root)
{
    auto stops = keypad::focusables(root);
    return stops.isEmpty() ? nullptr : stops.first();
}

FocusRing::FocusRing(QColor color, int thickness)
    : QWidget(nullptr)
    , color_(color)
    , thickness_(thickness)
{
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setFocusPolicy(Qt::NoFocus);
}

void FocusRing::track(QWidget *target)
{
    if (this->parentWidget() != target) {
        if (auto previous = this->parentWidget())
            previous->removeEventFilter(this);
        this->setParent(target);
        target->installEventFilter(this);
    }
    this->setGeometry(target->rect());
    this->raise();
    this->show();
    this->update();
}

void FocusRing::set_color(QColor color)
{
    this->color_ = color;
    this->update();
}

void FocusRing::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    auto fill = this->color_;
    fill.setAlphaF(.15);
    painter.fillRect(this->rect(), fill);

    int t = std::min({this->thickness_, this->width() / 2, this->height() / 2});
    painter.fillRect(0, 0, this->width(), t, this->color_);
    painter.fillRect(0, this->height() - t, this->width(), t, this->color_);
    painter.fillRect(0, 0, t, this->height(), this->color_);
    painter.fillRect(this->width() - t, 0, t, this->height(), this->color_);
}

bool FocusRing::eventFilter(QObject *object, QEvent *event)
{
    if (object == this->parentWidget() && event->type() == QEvent::Resize)
        this->setGeometry(this->parentWidget()->rect());
    return false;
}

KeypadNavigator::KeypadNavigator(Arbiter &arbiter, QWidget *window)
    : QObject(window)
    , arbiter_(arbiter)
    , window_(window)
{
    qApp->installEventFilter(this);
    auto filter = ActionEventFilter::get_instance();
    qApp->installEventFilter(filter);
    filter->set_fallback([this](QObject *receiver, QKeyEvent *event){ return this->handle(receiver, event); });

    connect(&arbiter, &Arbiter::curr_page_changed, this, [this](Page *page){
        if (!Dialog::top(this->window_) && page->button()->isVisible())
            page->button()->setFocus(Qt::OtherFocusReason);
        this->refresh();
    });
    connect(arbiter.layout().openauto_page, &QStackedWidget::currentChanged, this, [this]{ this->refresh(); });
    connect(&arbiter, &Arbiter::fullscreen_changed, this, [this]{ this->refresh(); });
    connect(&arbiter, &Arbiter::color_changed, this, [this](QColor color){
        if (this->ring_)
            this->ring_->set_color(color);
    });
}

KeypadNavigator::~KeypadNavigator()
{
    ActionEventFilter::get_instance()->set_fallback({});
    delete this->ring_;
}

bool KeypadNavigator::eventFilter(QObject *object, QEvent *event)
{
    switch (event->type()) {
        case QEvent::FocusIn:
            if (auto widget = qobject_cast<QWidget *>(object); widget && widget->window() == this->window_)
                this->refresh();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::TouchBegin:
            if (this->active_) {
                this->active_ = false;
                this->refresh();
            }
            break;
        default:
            break;
    }
    return false;
}

bool KeypadNavigator::handle(QObject *receiver, QKeyEvent *event)
{
    if (this->forward_target_)
        return receiver != this->forward_target_;
    if (event->type() != QEvent::KeyPress || !is_navigation(event->key()))
        return false;
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;

    auto widget = qobject_cast<QWidget *>(receiver);
    if (!widget || widget->window() != this->window_)
        return false;
    auto focus = this->window_->focusWidget();
    // only the first delivery, not its propagation to parents
    if (widget != (focus ? focus : this->window_))
        return false;
    if (QApplication::activePopupWidget() || this->arbiter_.android_auto_in_front())
        return false;
    if (focus && is_arrow(event->key()) && focus->property("keypad_arrows").toBool())
        return false;

    this->active_ = true;
    bool handled;
    if (auto dialog = Dialog::top(this->window_))
        handled = this->in_dialog(dialog, focus, event->key());
    else if (focus && this->rail().contains(focus))
        handled = this->in_rail(focus, event->key());
    else
        handled = this->in_content(focus, event->key());
    this->refresh();
    return handled;
}

bool KeypadNavigator::in_dialog(QWidget *dialog, QWidget *focus, int key)
{
    if (key == Qt::Key_Escape) {
        dialog->close();
        return true;
    }

    auto stops = keypad::focusables(dialog);
    if (!focus || !stops.contains(focus)) {
        if (!stops.isEmpty())
            this->focus(stops.first());
        return true;
    }

    if (is_arrow(key)) {
        if (focus && (qobject_cast<QAbstractItemView *>(focus) || qobject_cast<QTabBar *>(focus)) && this->offer(focus, key))
            return true;
        if (auto next = this->nearest(focus, key, stops))
            this->focus(next);
        return true;
    }
    return this->in_content(focus, key);
}

bool KeypadNavigator::in_rail(QWidget *focus, int key)
{
    auto rail = this->rail();
    switch (key) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown: {
            int step = (key == Qt::Key_Up || key == Qt::Key_PageUp) ? -1 : 1;
            auto idx = rail.indexOf(focus);
            this->focus(rail.at((idx + step + rail.size()) % rail.size()));
            return true;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Right: {
            auto page = this->page_of(focus);
            if (!page)
                return true;
            this->arbiter_.set_curr_page(page);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            if (this->arbiter_.android_auto_in_front())
                return true;
            if (auto first = keypad::first(page->container()))
                this->focus(first);
            return true;
        }
        default:
            return true;
    }
}

bool KeypadNavigator::in_content(QWidget *focus, int key)
{
    auto stops = this->content();
    if (!focus || (!stops.contains(focus) && !Dialog::top(this->window_))) {
        if (!this->to_rail() && !stops.isEmpty())
            this->focus(stops.first());
        return true;
    }

    switch (key) {
        case Qt::Key_Escape:
            if (this->arbiter_.layout().fullscreen.enabled)
                this->arbiter_.set_fullscreen(false);
            this->to_rail();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (auto button = qobject_cast<QAbstractButton *>(focus)) {
                button->click();
                return true;
            }
            if (auto combo = qobject_cast<QComboBox *>(focus)) {
                combo->showPopup();
                return true;
            }
            return false;
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            // the 1657 wheel sends PgDown for "+"
            return this->step_value(focus, (key == Qt::Key_PageDown) ? 1 : -1);
        default:
            break;
    }

    if ((qobject_cast<QAbstractItemView *>(focus) || qobject_cast<QTabBar *>(focus)) && this->offer(focus, key))
        return true;
    if (auto next = this->nearest(focus, key, stops))
        this->focus(next);
    else if (key == Qt::Key_Left)
        this->to_rail();
    return true;
}

bool KeypadNavigator::step_value(QWidget *focus, int step)
{
    if (auto slider = qobject_cast<QAbstractSlider *>(focus)) {
        slider->triggerAction((step > 0) ? QAbstractSlider::SliderPageStepAdd : QAbstractSlider::SliderPageStepSub);
        return true;
    }
    if (auto spin = qobject_cast<QAbstractSpinBox *>(focus)) {
        spin->stepBy(step);
        return true;
    }
    if (auto combo = qobject_cast<QComboBox *>(focus)) {
        combo->setCurrentIndex(std::clamp(combo->currentIndex() + step, 0, combo->count() - 1));
        return true;
    }
    if (auto bar = qobject_cast<QTabBar *>(focus)) {
        this->offer(bar, (step > 0) ? Qt::Key_Right : Qt::Key_Left);
        return true;
    }
    if (qobject_cast<QAbstractItemView *>(focus)) {
        this->offer(focus, (step > 0) ? Qt::Key_Down : Qt::Key_Up);
        return true;
    }
    // anything else: let it reach the widget, a scroll area above it scrolls
    return false;
}

bool KeypadNavigator::offer(QWidget *widget, int key)
{
    auto before = current_index(widget);
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    this->forward_target_ = widget;
    QCoreApplication::sendEvent(widget, &event);
    this->forward_target_ = nullptr;
    return current_index(widget) != before;
}

QWidget *KeypadNavigator::nearest(QWidget *from, int key, const QList<QWidget *> &candidates) const
{
    auto a = rect_in(from, this->window_);
    auto ac = QRectF(a).center();
    QWidget *best = nullptr;
    double best_score = std::numeric_limits<double>::max();

    for (auto candidate : candidates) {
        if (candidate == from || candidate->isAncestorOf(from) || from->isAncestorOf(candidate))
            continue;
        auto b = rect_in(candidate, this->window_);
        auto bc = QRectF(b).center();
        double primary, gap, offset;
        switch (key) {
            case Qt::Key_Down:
                if (b.top() <= a.top() || bc.y() <= ac.y())
                    continue;
                primary = bc.y() - ac.y();
                gap = std::max(0, std::max(a.left(), b.left()) - std::min(a.right(), b.right()));
                offset = std::abs(bc.x() - ac.x());
                break;
            case Qt::Key_Up:
                if (b.bottom() >= a.bottom() || bc.y() >= ac.y())
                    continue;
                primary = ac.y() - bc.y();
                gap = std::max(0, std::max(a.left(), b.left()) - std::min(a.right(), b.right()));
                offset = std::abs(bc.x() - ac.x());
                break;
            case Qt::Key_Right:
                if (b.left() <= a.left() || bc.x() <= ac.x())
                    continue;
                primary = bc.x() - ac.x();
                gap = std::max(0, std::max(a.top(), b.top()) - std::min(a.bottom(), b.bottom()));
                offset = std::abs(bc.y() - ac.y());
                break;
            case Qt::Key_Left:
                if (b.right() >= a.right() || bc.x() >= ac.x())
                    continue;
                primary = ac.x() - bc.x();
                gap = std::max(0, std::max(a.top(), b.top()) - std::min(a.bottom(), b.bottom()));
                offset = std::abs(bc.y() - ac.y());
                break;
            default:
                return nullptr;
        }
        double score = primary + 4 * gap + offset / 4;
        if (score < best_score) {
            best_score = score;
            best = candidate;
        }
    }
    return best;
}

QList<QWidget *> KeypadNavigator::rail() const
{
    QList<QWidget *> buttons;
    for (auto page : this->arbiter_.layout().pages()) {
        if (page->button()->isVisible())
            buttons.append(page->button());
    }
    return buttons;
}

QList<QWidget *> KeypadNavigator::content() const
{
    auto rail = this->rail();
    auto stops = keypad::focusables(this->window_);
    stops.erase(std::remove_if(stops.begin(), stops.end(), [&rail](QWidget *widget){ return rail.contains(widget); }), stops.end());
    return stops;
}

QWidget *KeypadNavigator::rail_button() const
{
    auto button = this->arbiter_.layout().curr_page->button();
    return button->isVisible() ? button : nullptr;
}

Page *KeypadNavigator::page_of(QWidget *button) const
{
    for (auto page : this->arbiter_.layout().pages()) {
        if (page->button() == button)
            return page;
    }
    return nullptr;
}

bool KeypadNavigator::to_rail()
{
    auto button = this->rail_button();
    if (button)
        this->focus(button);
    return button != nullptr;
}

void KeypadNavigator::focus(QWidget *widget)
{
    widget->setFocus(Qt::OtherFocusReason);
    for (auto parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto area = qobject_cast<QScrollArea *>(parent))
            area->ensureWidgetVisible(widget);
    }
    this->refresh();
}

void KeypadNavigator::refresh()
{
    auto target = this->window_->focusWidget();
    bool show = this->active_ && target && target != this->window_ && is_stop(target) &&
        !this->arbiter_.android_auto_in_front();
    if (!show) {
        if (this->ring_)
            this->ring_->hide();
        return;
    }
    if (!this->ring_) {
        int thickness = std::max(3, int(std::lround(3 * this->arbiter_.layout().scale)));
        this->ring_ = new FocusRing(this->arbiter_.theme().color(), thickness);
    }
    this->ring_->track(target);
}
