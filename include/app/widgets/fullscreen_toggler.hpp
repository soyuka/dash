#pragma once

#include <QElapsedTimer>
#include <QFrame>
#include <QLabel>
#include <QPoint>

class Arbiter;

class FullscreenToggler {
   public:
    FullscreenToggler(Arbiter &arbiter, QString name, QWidget *widget);
    virtual ~FullscreenToggler() = default;

    virtual void enable() = 0;
    virtual void disable() = 0;

    const QString &name() { return this->name_; }
    QWidget *widget() { return this->widget_; }

   protected:
    Arbiter &arbiter;

   private:
    const QString name_;
    QWidget *widget_;
};

class NullFullscreenToggler : public QFrame, public FullscreenToggler {
    Q_OBJECT

   public:
    NullFullscreenToggler(Arbiter &arbiter);

    void enable() override {}
    void disable() override {}
};

class BarFullscreenToggler : public QFrame, public FullscreenToggler {
    Q_OBJECT

   public:
    BarFullscreenToggler(Arbiter &arbiter);

    void enable() override { this->setVisible(true); }
    void disable() override { this->setVisible(false); }
};

// A draggable button floating over the main window, kept inside it.
class ButtonFullscreenToggler : public QFrame, public FullscreenToggler {
    Q_OBJECT

   public:
    ButtonFullscreenToggler(Arbiter &arbiter);

    void enable() override;
    void disable() override { this->hide(); }

   protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

   private:
    QPoint grab;
    QElapsedTimer touch;
    QLabel *label;

    void move_within_window(QPoint pos);
};
