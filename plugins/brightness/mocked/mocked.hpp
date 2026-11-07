#pragma once

#include <QObject>
#include <QPointer>
#include <QWidget>

#include "plugins/brightness_plugin.hpp"

// Software dimming for screens without a controllable backlight: a black,
// click-through overlay over the main window whose alpha follows brightness.
// Native video surfaces (the Android Auto QVideoWidget) are not covered.
class Mocked : public QObject, BrightnessPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID BrightnessPlugin_iid FILE "mocked.json")
    Q_INTERFACES(BrightnessPlugin)

   public:
    Mocked();
    ~Mocked() override;
    bool supported() override;
    uint8_t priority() override;
    void set(int brightness) override;

   private:
    QPointer<QWidget> overlay;
};
