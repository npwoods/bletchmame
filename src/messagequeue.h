/***************************************************************************

    messagequeue.h

    Generic thread safe message queue (equivalent to wxMessageQueue in
    wxWidgets)

***************************************************************************/

#pragma once

#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include <QSemaphore>
#include <QMutex>
#include <queue>

template<class T>
class MessageQueue
{
public:
    void post(T &&message)
    {
        QMutexLocker locker(&m_mutex);
        m_queue.push(std::move(message));
        m_semaphore.release();
    }

    T receive()
    {
        m_semaphore.acquire();
        QMutexLocker locker(&m_mutex);
        T result = std::move(m_queue.front());
        m_queue.pop();
        return result;
    }

private:
    std::queue<T>   m_queue;
    QMutex          m_mutex;
    QSemaphore      m_semaphore;
};

#endif // MESSAGEQUEUE_H
