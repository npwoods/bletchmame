/***************************************************************************

	throughputtracker.cpp

	Code for measuring throughput (used to measure auditing speeds)

***************************************************************************/

#include <QFile>
#include <QTextStream>

#include "throughputtracker.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

using namespace std::chrono_literals;

static const std::chrono::seconds TIME_WINDOW = 10s;



//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ThroughputTracker::ThroughputTracker(const QString &fileName)
	: m_fileName(fileName)
	, m_totalUnits(0)
	, m_throttler(1s)
{
}


//-------------------------------------------------
//  mark
//-------------------------------------------------

void ThroughputTracker::mark(double units)
{
	// get the current time
	Throttler::interval_t interval = Throttler::now();

	// pop items that are out of the window
	while (!m_queue.empty() && (interval - m_queue.front().m_interval) > TIME_WINDOW)
	{
		m_totalUnits -= m_queue.front().m_units;
		m_queue.pop();
	}

	// push a new item
	Entry e;
	e.m_interval = interval;
	e.m_units = units;
	m_queue.push(std::move(e));

	// bump the units
	m_totalUnits += units;

	// dump if appropriate
	if (m_throttler.check(interval))
		dump();
}


//-------------------------------------------------
//  dump
//-------------------------------------------------

void ThroughputTracker::dump() const
{
	QFile file(m_fileName);
	if (file.open(QIODeviceBase::WriteOnly))
	{
		QTextStream stream(&file);
		stream << m_totalUnits / (TIME_WINDOW / 1s) << " per second";
	}
}
