#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class Arbiter;

// Dialogs are overlays inside the main window rather than top-level windows:
// Wayland clients cannot position their own toplevels, and a single surface
// keeps the kiosk independent of compositor placement and focus policies.
class Dialog : public QWidget {
    Q_OBJECT

   public:
    Dialog(Arbiter &arbiter, bool fullscreen, QWidget *anchor = nullptr);
    void open(int timeout = 0);

    void set_title(QString str);
    void set_body(QWidget *widget);
    void set_button(QPushButton *button);

   protected:
    Arbiter &arbiter;

    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

   private:
    QPointer<QWidget> anchor;
    QFrame *frame;
    QVBoxLayout *title;
    QVBoxLayout *body;
    QHBoxLayout *buttons;
    QTimer *timer;
    bool fullscreen;

    QFrame *content_widget();
    void update_geometry();
    void add_cancel_button();
};

class SnackBar : public Dialog {
    Q_OBJECT

   public:
    SnackBar(Arbiter &arbiter);

   protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
};
