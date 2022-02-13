/***************************************************************************

	perfprofiler.h

	Code for performance profiling BletchMAME itself (not to be confused
	with profiles)

***************************************************************************/

#pragma once

#ifndef PERFPROFILER_H
#define PERFPROFILER_H

// bletchmame headers
#include "throttler.h"
#include "utility.h"

// Qt headers
#include <QThreadStorage>

// standard headers
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <stack>
#include <thread>
#include <unordered_map>
#include <vector>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#ifndef USE_PROFILER
#define USE_PROFILER	0
#endif // USE_PROFILER

#ifdef _MSC_VER
#define STRINGIFY1(x) #x
#define STRINGIFY2(x) STRINGIFY1(x)
#define CURRENT_FUNCTION	__FILE__ ":" STRINGIFY2(__LINE__)
#elif defined(__GNUC__)
#define CURRENT_FUNCTION	ProfilerLabel::fromPrettyFunction(__PRETTY_FUNCTION__)
#else
#define CURRENT_FUNCTION	"<<cannot identify function>>"
#endif


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> ProfilerLabel

class ProfilerLabel
{
	friend struct std::hash<ProfilerLabel>;

public:
	class Test;

	constexpr ProfilerLabel(const char *text)
	{
		bool b = true;
		for (auto i = 0; i < std::size(m_text); i++)
		{
			m_text[i] = b ? text[i] : '\0';
			if (text[i] == '\0')
				b = false;
		}
	}
	ProfilerLabel(const ProfilerLabel &) = default;
	ProfilerLabel(ProfilerLabel &&) = default;

	// operators
	bool operator==(const ProfilerLabel &that) const = default;

	std::u8string_view toStringView() const;
	QString toQString() const;

	static constexpr ProfilerLabel fromPrettyFunction(const char *text)
	{
		const char *newText = text;
		while (*text != '\0' && *text != '(')
		{
			if (*text == ' ')
				newText = text + 1;
			text++;
		}
		return ProfilerLabel(newText);
	}

private:
	std::array<char, 128> m_text;
};


namespace std
{
	template<>
	struct hash<ProfilerLabel>
	{
		constexpr std::size_t operator()(const ProfilerLabel &x) const
		{
			return util::array_hash(x.m_text);
		}
	};
}


// ======================> RealPerformanceProfiler

class RealPerformanceProfiler
{
public:
	const int MAX_LABEL_SIZE = 64;

	// ctor/dtor
	RealPerformanceProfiler(const char *fileName);
	RealPerformanceProfiler(const RealPerformanceProfiler &) = delete;
	RealPerformanceProfiler(RealPerformanceProfiler &&) = delete;
	~RealPerformanceProfiler();

	// statics
	static RealPerformanceProfiler *current();

	// methods
	void ping();
	void startScope(ProfilerLabel label);
	void endScope();

private:
	struct Entry
	{
		Entry(QString &&label);

		QString			m_label;
		std::clock_t	m_accumulatedTime;
		int				m_count;
	};

	const char *									m_fileName;
	QString											m_fullFileName;
	Throttler										m_throttler;
	std::clock_t									m_globalStartTime;
	std::size_t										m_currentLabelIndex;
	std::clock_t									m_currentStartTime;
	std::unordered_map<ProfilerLabel, std::size_t>	m_labelMap;
	std::stack<std::size_t>							m_stack;
	std::vector<Entry>								m_entries;
	
	static QThreadStorage<std::optional<std::reference_wrapper<RealPerformanceProfiler>>>	s_instance;

	// private methods
	std::size_t getLabelIndex(ProfilerLabel label);
	void recordAccumulatedTime(std::size_t labelIndex);
	std::size_t introduceNewLabel(const ProfilerLabel &label);
	void dump(QIODevice &file);
	void dump(QTextStream &file);
};


// ======================> FakePerformanceProfiler

class FakePerformanceProfiler
{
public:
	FakePerformanceProfiler(const char *fileName)
	{
	}
	FakePerformanceProfiler(const FakePerformanceProfiler &) = delete;
	FakePerformanceProfiler(FakePerformanceProfiler &&) = delete;

	static FakePerformanceProfiler *current()	{ return nullptr; }
	void startScope(ProfilerLabel)				{ /* do nothing */ }
	void endScope()								{ /* do nothing */ }
	void ping()									{ /* do nothing */ }
};


// ======================> PerformanceProfiler

#if USE_PROFILER
typedef RealPerformanceProfiler PerformanceProfiler;
#else // !USE_PROFILER
typedef FakePerformanceProfiler PerformanceProfiler;
#endif // USE_PROFILER


// ======================> ProfilerScope

class ProfilerScope
{
public:
	ProfilerScope(ProfilerLabel label)
	{
		if (PerformanceProfiler::current())
			PerformanceProfiler::current()->startScope(label);
	}

	~ProfilerScope()
	{
		if (PerformanceProfiler::current())
			PerformanceProfiler::current()->endScope();
	}
};


#endif // PERFPROFILER_H
