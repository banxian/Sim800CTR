#include "KeyItem.h"



TKeyItem::TKeyItem( int ID, const std::string& graphic, int matchedkeycode )
    : fRow(ID / 10)
    , fColumn(ID % 10)
    , fPressed(false)
    , fHold(false)
    , fGraphic(graphic)
{
    addKeycode(matchedkeycode);
}

void TKeyItem::addKeycode( int matchedkeycode )
{
    // HID
    if (matchedkeycode) {
        fMatchedKeycodes.insert(matchedkeycode);
    }
}

void TKeyItem::setRect( const QRect& rect )
{
    fRect = rect;
    // TODO: change font size

}

void TKeyItem::setSubscript( const std::string& subscript )
{
    fSubscript = subscript;
}

bool TKeyItem::inRect( const QPoint& point )
{
    return fRect.contains(point);
}

void setRGB(demo::RenderSystemDrawing & render, unsigned rgb)
{
    render.SetColor((rgb >> 16) / 255.0, (rgb >> 8 & 0xFF)/ 255.0, (rgb & 0xFF)/ 255.0);

}

void TKeyItem::paintSelf( demo::RenderSystemDrawing & render )
{
    if (fRect.isEmpty()) {
        return;
    }

    unsigned framecolor;
    unsigned bgcolor;
    if (fHold) {
        framecolor = 0x14C906;
        bgcolor = 0x9AC986;
    } else if (fPressed) {
        framecolor = 0x404906;
        bgcolor = 0xBABABA; //Qt::lightGray;
    } else {
        framecolor = 0x80C946;
        bgcolor = 0xD9E9CD;
    }
    //framebrush.setStyle(Qt::SolidPattern);
    //painter.setRenderHint(QPainter::Antialiasing);
    ////painter.setOpacity(0.5);
    //QBrush oldbrush = painter.brush();
    ////framebrush.setStyle(Qt::NoBrush);
    //framebrush.setColor(bgcolor);
    //painter.setBrush(framebrush);
    //painter.drawRoundedRect(fRect, 4, 4, Qt::AbsoluteSize);
    setRGB(render, bgcolor);
    render.FillRectangle(fRect.x, fRect.y, fRect.w, fRect.h);
    setRGB(render, framecolor);
    render.DrawLine(fRect.x, fRect.y, fRect.x + fRect.w, fRect.y);
    render.DrawLine(fRect.x, fRect.y + fRect.h, fRect.x + fRect.w, fRect.y + fRect.h);
    render.DrawLine(fRect.x, fRect.y, fRect.x, fRect.y + fRect.h);
    render.DrawLine(fRect.x + fRect.w, fRect.y, fRect.x + fRect.w, fRect.y + fRect.h);
    if (fSubscript.empty() == false) {
        //QPolygon subbg;
        //subbg.append(QPoint(fRect.x() + fRect.width() * 2 / 3 + 2, fRect.y() + fRect.height() * 2 / 3));
        //subbg.append(QPoint(fRect.x() + fRect.width(), fRect.y() + fRect.height() * 2 / 3));
        //subbg.append(QPoint(fRect.x() + fRect.width(), fRect.y() + fRect.height() - 2));
        //subbg.append(QPoint(fRect.x() + fRect.width() - 2, fRect.y() + fRect.height()));
        //subbg.append(QPoint(fRect.x() + fRect.width() * 2 / 3 - 2, fRect.y() + fRect.height()));
        //framebrush.setColor(painter.pen().color());
        //// same color as frame
        //painter.setBrush(framebrush);
        //painter.drawPolygon(subbg);
    }
    //render.SetColor(1.0, 1.0, 1.0); // white
    //painter.drawText(fRect, Qt::AlignCenter | Qt::AlignHCenter | Qt::TextWrapAnywhere, fGraphic);
    render.DrawText(fRect.x + 2, fRect.y + 2, "%s", fGraphic.c_str());
    if (fSubscript.empty() == false) {
        //painter.setPen(QPen(QColor(0xFFFDE8), 2, Qt::SolidLine));
        //painter.drawText(fRect.adjusted(2,2,-2,-2), Qt::AlignRight | Qt::AlignBottom | Qt::TextWrapAnywhere, fSubscript);
    }
    //painter.setBrush(oldbrush);
}

bool TKeyItem::press( int keycode )
{
    for (std::set<int>::const_iterator it = fMatchedKeycodes.begin(); it != fMatchedKeycodes.end(); it++) {
        if ((*it & keycode)) {
            fPressed = true;
            return true;
        }
    }
    return false;
}

void TKeyItem::press()
{
    fPressed = true;
}

bool TKeyItem::release( int keycode )
{
    for (std::set<int>::const_iterator it = fMatchedKeycodes.begin(); it != fMatchedKeycodes.end(); it++) {
        if ((*it & keycode)) {
            fPressed = false;
            return true;
        }
    }
    return false;
}

void TKeyItem::release()
{
    fPressed = false;
}

bool TKeyItem::pressed()
{
    return fPressed;
}

void TKeyItem::hold()
{
    fPressed = true;
    fHold = true;
}

int TKeyItem::row()
{
    return fRow;
}

int TKeyItem::column()
{
    return fColumn;
}
