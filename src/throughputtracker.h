/***************************************************************************

	throughputtracker.h

	Code for measuring throughput (used to measure auditing speeds)

***************************************************************************/

#pragma once

#ifndef THROUGHPUTTRACKER_H
#define THROUGHPUTTRACKER_H

#include <queue>

#include <QString>

#include "throttler.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> ThroughputTracker

class ThroughputTracker
{
public:
	// ctor
	ThroughputTracker(const QString &fileName);
	ThroughputTracker(const ThroughputTracker &) = delete;
	ThroughputTracker(ThroughputTracker &&) = delete;

	// methods
	void mark(double units);

private:
	struct Entry
	{
		Throttler::interval_t	m_interval;
		double					m_units;
	};

	// members
	QString						m_fileName;
	double						m_totalUnits;
	Throttler					m_throttler;
	std::queue<Entry>			m_queue;

	// private methods
	void dump() const;
};


#endif // THROUGHPUTTRACKER_H
