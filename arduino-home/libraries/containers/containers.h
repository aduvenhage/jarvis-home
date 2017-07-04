#ifndef CONTAINERS_H
#define CONTAINERS_H
#include <Arduino.h>



/// Round up to next higher power of 2 (return s if it's already a power of 2).
#define POW2SIZE(s) (((s-1) | ((s-1) >> 1) | ((s-1) >> 2) | ((s-1) >> 4) | ((s-1) >> 8)) + 1)



/// ringbuffer queue. The ring buffer size will always be a power of 2 (pow2(S) <= size)
template <typename T, size_t S>
class Queue
{
 public:
    Queue()
        :m_uBegin(0),
         m_uEnd(0),
         m_uSizeMask(POW2SIZE(S)-1)
    {}
    
    ~Queue()
    {}
    
    void push(const T &_rData)
    {
        m_data[m_uEnd & m_uSizeMask] = _rData;
        m_uEnd++;
    }
    
    T &pop(T &_rData)
    {
        if (empty() == false)
        {
            _rData = m_data[m_uBegin & m_uSizeMask];
            m_uBegin++;
        }
        
        return _rData;
    }
    
    bool empty() const
    {
        return m_uBegin == m_uEnd;
    }
    
    size_t size() const {return m_uSizeMask+1;}
    
 private:
    T                   m_data[POW2SIZE(S)];
    const size_t        m_uSizeMask;
    size_t              m_uBegin;
    size_t              m_uEnd;
};






#endif  // #ifndef CONTAINERS_H

