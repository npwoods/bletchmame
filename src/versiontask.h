/***************************************************************************

    versiontask.h

    Task for invoking '-version'

***************************************************************************/

#pragma once

#ifndef VERSIONTASK_H
#define VERSIONTASK_H

#include <QEvent>

#include "task.h"


//**************************************************************************
//  MACROS
//**************************************************************************

// pesky macros
#ifdef ERROR
#undef ERROR
#endif // ERROR


//**************************************************************************
//  TYPES
//**************************************************************************

class VersionResultEvent : public QEvent
{
public:
    VersionResultEvent(QString &&version);

    static QEvent::Type eventId() { return s_eventId; }

    QString	m_version;

private:
    static QEvent::Type s_eventId;
};


//**************************************************************************
//  FUNCTION PROTOTYPES
//**************************************************************************

Task::ptr createVersionTask();

#endif // VERSIONTASK_H
