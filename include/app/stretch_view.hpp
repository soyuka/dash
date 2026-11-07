#pragma once

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QSize>

class StretchView : public QGraphicsView {
    Q_OBJECT

   public:
    StretchView(QWidget *content, QSize logical_size, QWidget *parent = nullptr);
    QGraphicsProxyWidget *proxy() const { return this->proxy_; }
    static QWidget *top_level(QWidget *widget);

   protected:
    void resizeEvent(QResizeEvent *event) override;

   private:
    QGraphicsScene scene_;
    QSize logical_size_;
    QGraphicsProxyWidget *proxy_;
};
