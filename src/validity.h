/***************************************************************************

    validity.h

    Validity checks

***************************************************************************/

#pragma once

#ifndef VALIDITY_H
#define VALIDITY_H

#include <functional>
#include <optional>
#include <string_view>


//**************************************************************************
//  VALIDITY CHECK CLASSES
//**************************************************************************

// ======================> real_validity_check
class real_validity_check
{
public:
	real_validity_check(std::function<void()> &&func)
		: m_func(std::move(func))
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
	template<typename T>
	fake_validity_check(T func)
	{
		(void)func;
	}

	static bool run_all() { return true; }
};


// ======================> validity_check
#ifdef _DEBUG
typedef real_validity_check	validity_check;
#else
typedef fake_validity_check	validity_check;
#endif // _DEBUG


//**************************************************************************
//  TEST ASSET LOADING
//**************************************************************************

std::optional<std::string_view> load_test_asset(const char *asset_name);

#endif // VALIDITY_H
