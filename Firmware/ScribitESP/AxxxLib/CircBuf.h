#pragma once
#include <stdexcept>

/* 
   T must implement operator=, copy ctor 
*/

template <typename T>
class CircBuf
{
  // don't use default ctor
  CircBuf();

  const int size;
  T *data;
  int front;
  int count;

public:
  CircBuf(int);
  ~CircBuf();

  bool empty() { return count == 0; }
  int length() { return count; }
  bool full() { return count == size; }
  bool add(const T &);
  bool remove(T *);
  int dump(T *);
  bool peek(T *t, int index);
  bool peekBack(T *t, int indexFromLast);
  void flush() {count=0; front=0;}
};

template <typename T>
CircBuf<T>::CircBuf(int sz) : size(sz)
{
  if (sz == 0)
    throw std::invalid_argument("size cannot be zero");
  data = new T[sz];
  front = 0;
  count = 0;
}
template <typename T>
CircBuf<T>::~CircBuf()
{
  delete data;
}

// returns true if add was successful, false if the buffer is already full
template <typename T>
bool CircBuf<T>::add(const T &t)
{
  if (full())
  {
    return false;
  }
  else
  {
    // find index where insert will occur
    int end = (front + count) % size;
    data[end] = t;
    count++;
    return true;
  }
}

// returns true if there is something to remove, false otherwise
template <typename T>
bool CircBuf<T>::remove(T *t)
{
  if (empty())
  {
    return false;
  }
  else
  {
    *t = data[front];
    front++;
    if (front >= size)
      front = 0;
    count--;
    return true;
  }
}

// returns true if there is something do not delete from buffer
template <typename T>
bool CircBuf<T>::peek(T *t, int index)
{
  if (empty() || index>=count)
  {
    return false;
  }
  else
  {
    *t = data[(front + index) % size];
    return true;
  }
}

// returns true if there is something do not delete from buffer
template <typename T>
bool CircBuf<T>::peekBack(T *t, int indexFromLast)
{
  if (empty() || indexFromLast>=count)
  {
    return false;
  }
  else
  {
    int last = (front + count -1) % size;
    int toSend=last-indexFromLast;
    if(toSend<0)
      toSend+=size;
    *t = data[toSend];
    return true;
  }
}

//Dumps all data in an array and returns length
template <typename T>
int CircBuf<T>::dump(T *array)
{
  for (int i = 0; i < count; i++)
  {
    array[i] = data[(front + i) % size];
  }

  return count;
}