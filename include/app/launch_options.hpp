#pragma once

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QSize>
#include <QString>

class LaunchOptions {
   public:
    LaunchOptions();
    void add_to(QCommandLineParser &parser) const;
    void resolve(const QCommandLineParser &parser, const QSettings &settings);

    QSize window_size;
    QSize logical_size;
    QString error;

   private:
    QCommandLineOption windowed;
    QCommandLineOption logical;
};
