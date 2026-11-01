#pragma once
#include <QPushButton>
#include <QPixmap>
#include <QPainter>
#include <QEvent>

// QPushButton drawn with the original Novetus button art
// (normal / _hover / _click PNGs from the Stylish launcher).
class ImageButton : public QPushButton
{
    Q_OBJECT
public:
    ImageButton(const QString &assetBase, const QString &toolTip, QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        m_normal.load(":/assets/" + assetBase + ".png");
        m_hover.load(":/assets/" + assetBase + "_hover.png");
        m_pressed.load(":/assets/" + assetBase + "_click.png");
        setFixedSize(m_normal.size());
        setToolTip(toolTip);
        setFlat(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const QPixmap *img = &m_normal;
        if (m_pressedNow && !m_pressed.isNull()) img = &m_pressed;
        else if (m_hovering && !m_hover.isNull()) img = &m_hover;
        if (!img->isNull()) p.drawPixmap(rect(), *img);
    }

    bool event(QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::HoverEnter:
        case QEvent::Enter:
            m_hovering = true; update(); break;
        case QEvent::HoverLeave:
        case QEvent::Leave:
            m_hovering = false; update(); break;
        case QEvent::MouseButtonPress:
            m_pressedNow = true; update(); break;
        case QEvent::MouseButtonRelease:
            m_pressedNow = false; update(); break;
        default: break;
        }
        return QPushButton::event(e);
    }

private:
    QPixmap m_normal, m_hover, m_pressed;
    bool m_hovering = false;
    bool m_pressedNow = false;
};
