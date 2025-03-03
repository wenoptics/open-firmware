#pragma once
#include "CircBuf.h"

template <typename T>
class CircBufInfinite : public CircBuf<T>
{
    CircBufInfinite();

  public:
    CircBufInfinite(int sz);
    bool add(const T &);
};

template <typename T>
CircBufInfinite<T>::CircBufInfinite(int sz) : CircBuf<T>(sz){};

template <typename T>
bool CircBufInfinite<T>::add(const T &t)
{
    T temp;
    //If full drop oldest
    if (CircBuf<T>::full())
        CircBuf<T>::remove(&temp);
    
    //Add new value
    return CircBuf<T>::add(t);

    
}