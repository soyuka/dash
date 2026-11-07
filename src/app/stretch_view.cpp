#include "app/stretch_view.hpp"

StretchView::StretchView(QWidget *content, QSize logical_size, QWidget *parent)
    : QGraphicsView(parent)
    , scene_()
    , logical_size_(logical_size)
    , proxy_(nullptr)
{
    this->setFrameShape(QFrame::NoFrame);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    this->setTransformationAnchor(QGraphicsView::NoAnchor);
    this->setResizeAnchor(QGraphicsView::NoAnchor);
    this->setBackgroundBrush(Qt::black);

    QRectF rect(QPointF(0, 0), logical_size);
    this->scene_.setSceneRect(rect);
    this->setScene(&this->scene_);

    content->setFixedSize(logical_size);
    this->proxy_ = this->scene_.addWidget(content);
    this->proxy_->setGeometry(rect);
    this->proxy_->setAcceptTouchEvents(true);
    this->proxy_->setFocus();
}

void StretchView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    auto size = this->viewport()->size();
    this->setTransform(QTransform::fromScale(double(size.width()) / this->logical_size_.width(),
                                             double(size.height()) / this->logical_size_.height()));
}

QWidget *StretchView::top_level(QWidget *widget)
{
    auto window = widget->window();
    if (auto proxy = window->graphicsProxyWidget()) {
        if (auto scene = proxy->scene(); scene && !scene->views().isEmpty())
            return scene->views().first()->window();
    }
    return window;
}
