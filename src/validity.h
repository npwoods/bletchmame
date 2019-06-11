/***************************************************************************

    validity.h

    Validity checks

***************************************************************************/

#pragma once

#ifndef VALIDITY_H
#define VALIDITY_H

#include <functional>

// ======================> real_validity_check
class real_validity_check
{
public:
	real_validity_check(void(*func)())
		: m_func(func)
	{
		setup();
	}

	static bool run_all();

private:
	std::function<void()> m_func;

	void setup() const;
	void execute() const;
};


// ======================> fake_validity_check
class fake_validity_check
{
public:
	fake_validity_check(void (*)()) { }

	static bool run_all() { return true; }
};


// ======================> validity_check
#ifdef _DEBUG
typedef real_validity_check	validity_check;
#else
typedef fake_validity_check	validity_check;
#endif // _DEBUG

#endif // VALIDITY_H
