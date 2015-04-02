#ifndef KEYITEM_H
#define KEYITEM_H

#include <nn.h>
#include <string>
#include <set>
#include "demo.h"
#include "CheatTypes.h"

#include "../Common/FsSampleCommon.h"




//template<class T> inline
//bool contains(const std::set<T>& container, const T& value)
//{
//    return container.find(value) != container.end();
//}

class TKeyItem {
public:
    explicit TKeyItem(int ID, const std::string& graphic, int matchedkeycode);
private:
    int fRow;
    int fColumn;
    bool fPressed;
    bool fHold;
    NN_PADDING2;
    std::string fGraphic;
    std::string fSubscript;
    std::set<int> fMatchedKeycodes;
    QRect fRect;
public:
    void addKeycode(int matchedkeycode);
    void setRect(const QRect& rect);
    void setSubscript(const std::string& subscript);
    bool inRect(const QPoint& point);
    void paintSelf(demo::RenderSystemDrawing & render);
    bool press(int keycode);
    bool release(int keycode);
    void hold();
    void press();
    void release();
    bool pressed();
    int row();
    int column();
};

#endif // KEYITEM_H
