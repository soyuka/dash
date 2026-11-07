#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QWidget>

class Arbiter;
class Page;
class QKeyEvent;

namespace keypad {
// Visible, enabled widgets under root that a keypad can land on, in reading order.
// Containers holding such widgets (scroll areas) are not stops themselves.
QList<QWidget *> focusables(QWidget *root);
QWidget *first(QWidget *root);
}

// Focus indicator drawn as a child of the focused widget, so it follows it
// through scrolling and never takes part in layouts.
class FocusRing : public QWidget {
    Q_OBJECT

   public:
    FocusRing(QColor color, int thickness);
    void track(QWidget *target);

    QColor color() const { return this->color_; }
    void set_color(QColor color);
    int thickness() const { return this->thickness_; }

   protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

   private:
    QColor color_;
    int thickness_;
};

// Keypad navigation of dash's own UI (see docs/keypad.md). Keys reach it through
// ActionEventFilter once no available action claimed them.
class KeypadNavigator : public QObject {
    Q_OBJECT

   public:
    KeypadNavigator(Arbiter &arbiter, QWidget *window);
    ~KeypadNavigator();
    bool handle(QObject *receiver, QKeyEvent *event);

   protected:
    bool eventFilter(QObject *object, QEvent *event) override;

   private:
    Arbiter &arbiter_;
    QWidget *window_;
    QPointer<FocusRing> ring_;
    QPointer<QWidget> forward_target_;
    bool active_ = false;

    bool in_dialog(QWidget *dialog, QWidget *focus, int key);
    bool in_rail(QWidget *focus, int key);
    bool in_content(QWidget *focus, int key);

    QList<QWidget *> rail() const;
    QList<QWidget *> content() const;
    QWidget *rail_button() const;
    Page *page_of(QWidget *button) const;
    bool to_rail();
    bool step_value(QWidget *focus, int step);
    bool offer(QWidget *widget, int key);
    QWidget *nearest(QWidget *from, int key, const QList<QWidget *> &candidates) const;
    void focus(QWidget *widget);
    void refresh();
};
