/***************************************************************************

	perfprofiler.cpp

	Code for performance profiling BletchMAME itself (not to be confused
	with profiles)

***************************************************************************/

#include <QCoreApplication>
#include <QFile>
#include <numeric>

#include "perfprofiler.h"

using namespace std::chrono_literals;

QThreadStorage<std::optional<std::reference_wrapper<RealPerformanceProfiler>>> RealPerformanceProfiler::s_instance;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RealPerformanceProfiler::RealPerformanceProfiler(const char *fileName)
	: m_fileName(fileName)
	, m_throttler(500ms)
{
	// we can't run this multiple times per thread
	if (current())
		throw false;

	// specify the instance
	s_instance.setLocalData(std::reference_wrapper<RealPerformanceProfiler>(*this));

	// prserve entries
	const int RESERVE_LABEL_COUNT = 256;
	m_labelMap.reserve(RESERVE_LABEL_COUNT);
	m_entries.reserve(RESERVE_LABEL_COUNT);

	// initial setup
	m_currentLabelIndex = getLabelIndex("<<none>>");
	m_currentStartTime = std::clock();
	m_stack.push(m_currentLabelIndex);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

RealPerformanceProfiler::~RealPerformanceProfiler()
{
	if (!m_fullFileName.isEmpty())
	{
		QFile file(m_fullFileName);
		file.open(QIODevice::WriteOnly);
	}
	s_instance.setLocalData(std::nullopt);
}


//-------------------------------------------------
//  current
//-------------------------------------------------

RealPerformanceProfiler *RealPerformanceProfiler::current()
{
	return s_instance.hasLocalData() && s_instance.localData().has_value()
		? &((RealPerformanceProfiler &) s_instance.localData().value())
		: nullptr;
}


//-------------------------------------------------
//  startScope
//-------------------------------------------------

void RealPerformanceProfiler::startScope(ProfilerLabel label)
{
	// translate the label to an index and push it on the stack
	std::size_t labelIndex = getLabelIndex(label);
	m_stack.push(labelIndex);

	// record accumulated time
	recordAccumulatedTime(labelIndex);

	// bump the count
	m_entries[labelIndex].m_count++;

	// and ping
	ping();
}


//-------------------------------------------------
//  endScope
//-------------------------------------------------

void RealPerformanceProfiler::endScope()
{
	// pop a label off the stack
	m_stack.pop();
	std::size_t labelIndex = m_stack.top();

	// record accumulated time
	recordAccumulatedTime(labelIndex);

	// and ping
	ping();
}


//-------------------------------------------------
//  getLabelIndex
//-------------------------------------------------

std::size_t RealPerformanceProfiler::getLabelIndex(ProfilerLabel label)
{
	auto iter = m_labelMap.find(label);
	return iter == m_labelMap.end()
		? introduceNewLabel(label)
		: iter->second;
}


//-------------------------------------------------
//  recordAccumulatedTime
//-------------------------------------------------

void RealPerformanceProfiler::recordAccumulatedTime(std::size_t labelIndex)
{
	// get the current clock
	std::clock_t currentClock = std::clock();

	// record accumulated time for the label we're currently tracking
	std::clock_t accumulatedTime = currentClock - m_currentStartTime;
	m_entries[m_currentLabelIndex].m_accumulatedTime += accumulatedTime;

	// and start tracking this one
	m_currentLabelIndex = labelIndex;
	m_currentStartTime = currentClock;
}


//-------------------------------------------------
//  introduceNewLabel
//-------------------------------------------------

std::size_t RealPerformanceProfiler::introduceNewLabel(const ProfilerLabel &label)
{
	QString labelText = label.toQString().leftJustified(70);

	std::size_t result = m_labelMap.size();
	m_labelMap.emplace(label, result);
	m_entries.emplace_back(std::move(labelText));
	return result;
}


//-------------------------------------------------
//  ping
//-------------------------------------------------

void RealPerformanceProfiler::ping()
{
	if (m_throttler.check())
	{
		// if we have not evaluated the full file name, do so now
		if (m_fullFileName.isEmpty())
		{
			QString appDirPath = QCoreApplication::applicationDirPath();
			if (!appDirPath.isEmpty())
				m_fullFileName = QString("%1/%2").arg(appDirPath, m_fileName);
		}

		// and dump
		if (!m_fullFileName.isEmpty())
		{
			QFile file(m_fullFileName);
			if (file.open(QIODeviceBase::WriteOnly))
				dump(file);
		}
	}
}


//-------------------------------------------------
//  dump(QIODevice &)
//-------------------------------------------------

void RealPerformanceProfiler::dump(QIODevice &file)
{
	QTextStream stream(&file);
	dump(stream);
}


//-------------------------------------------------
//  dump(QTextStream &)
//-------------------------------------------------

void RealPerformanceProfiler::dump(QTextStream &stream)
{
	// accumulate time one last time
	recordAccumulatedTime(m_currentLabelIndex);

	// get the total time
	std::clock_t totalTime = 0;
	for (const Entry &entry : m_entries)
		totalTime += entry.m_accumulatedTime;

	if (totalTime > 0)
	{
		for (const Entry &entry : m_entries)
		{
			QString percent = QString::number((double)entry.m_accumulatedTime / totalTime * 100.0);
			stream << QString("%1\t%2 / %3%\n").arg(
				entry.m_label,
				QString::number(entry.m_count),
				percent);
		}
	}
	else
	{
		stream << "(no actively profiled code found)\n";
	}

	// clear out times
	for (Entry &entry : m_entries)
	{
		entry.m_accumulatedTime = 0;
		entry.m_count = 0;
	}
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

RealPerformanceProfiler::Entry::Entry(QString &&label)
	: m_label(std::move(label))
	, m_accumulatedTime(0)
	, m_count(0)
{
}


//-------------------------------------------------
//  ProfilerLabel::toQString()
//-------------------------------------------------

QString ProfilerLabel::toQString() const
{
	std::size_t len = 0;
	while (len < std::size(m_text) && m_text[len])
		len++;
	std::u8string_view sv((const char8_t *) &m_text[0], len);
	return util::toQString(sv);
}
