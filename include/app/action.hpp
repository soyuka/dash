#pragma once

#include <QCloseEvent>
#include <QFile>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QLabel>
#include <QObject>
#include <QRegularExpression>
#include <QShortcut>
#include <QString>
#include <QShowEvent>
#include <QWidget>
#include <functional>
#include <mutex>
#include <QSet>


#include "app/widgets/dialog.hpp"

class Arbiter;

class GPIONotifier : public QObject {
    Q_OBJECT

   public:
    static const QRegularExpression GPIOX_REGEX;
    static const QString GPIO_DIR;
    static const QString GPIOX_DIR;
    static const QString GPIOX_VALUE_PATH;
    static const QString GPIOX_ACTIVE_LOW_PATH;

    GPIONotifier();

    void enable() { this->watcher.blockSignals(false); }
    void disable() { this->watcher.blockSignals(true); }

   private:
    QFileSystemWatcher watcher;

   signals:
    void triggered(QString gpio);
};

class ActionDialog : public Dialog {
    Q_OBJECT

   public:
    ActionDialog(Arbiter &arbiter);

    QString key() { return label->text(); }

   protected:
    void keyPressEvent(QKeyEvent *event);
    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent *event);

   private:
    GPIONotifier notifier;
    QLabel *label;
};

class Action : public QObject {
    Q_OBJECT

   public:
    enum ActionState
    {
        Activated,
        Deactivated,
        Triggered
    };
    Q_ENUM(ActionState)

    Action(QString id, QString name, std::function<void(ActionState)> func, QWidget *parent,
           std::function<bool()> available = {});
    void set(QString key);
    void fire(ActionState state);
    bool available() const { return !this->available_ || this->available_(); }

    QString id() const { return this->id_; }
    QString key() const { return this->key_; }
    QString name() const { return this->name_; }

   signals:
    void fired(Action::ActionState state);

   private:
    struct GPIO {
        QFileSystemWatcher watcher;
        QFile value;
        uint8_t active_low;

        GPIO();
        ~GPIO();
    };

    std::function<void(ActionState)> func_;
    std::function<bool()> available_;
    QShortcut shortcut;
    GPIO gpio;

    QString id_;
    QString name_;
    QString key_;
};

// Single entry point for key input: a key bound to an available action fires
// it, anything else goes to the fallback (keypad navigation).
class ActionEventFilter : public QObject
{
    Q_OBJECT

   public:
    using Fallback = std::function<bool(QObject *, QKeyEvent *)>;

    ActionEventFilter(){};
    bool eventFilter(QObject* obj, QEvent* event);
    static ActionEventFilter *get_instance();
    QMap<int, Action*> eventFilterMap;

    void enable() { this->disabled = false; }
    void disable() { this->disabled = true; }
    void set_fallback(Fallback fallback) { this->fallback_ = std::move(fallback); }

   private:
    std::mutex mutex_;
    bool disabled = false;
    QSet<int> held_;
    Fallback fallback_;
};
