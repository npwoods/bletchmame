/***************************************************************************

    validity.cpp

    Miscellaneous utility code

***************************************************************************/

#include <list>

#include "validity.h"
#include "utility.h"


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

static std::list<const real_validity_check *> *s_checks_ptr;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  setup
//-------------------------------------------------

void real_validity_check::setup() const
{
	static std::list<const real_validity_check *> s_checks;
	s_checks_ptr = &s_checks;
	s_checks.push_back(this);
}


//-------------------------------------------------
//  execute
//-------------------------------------------------

void real_validity_check::execute() const
{
	m_func();
}



//-------------------------------------------------
//  run_all
//-------------------------------------------------

bool real_validity_check::run_all()
{
	for (const real_validity_check *check : *s_checks_ptr)
		check->execute();
	return true;
}


//-------------------------------------------------
//  load_test_asset
//-------------------------------------------------

std::optional<std::string_view> load_test_asset(const char *asset_name)
{
	// this is returning optional so that in the case of a non-Windows OS where we have not
	// gotten resources working, this can fail benignly (still not the right long term solution)
	std::optional<std::string_view> result;

#ifdef __WINDOWS__
	const void *buffer;
	size_t buffer_length;
	bool success = wxLoadUserResource(&buffer, &buffer_length, wxString::Format("testasset_%s", asset_name));
	if (!success || !buffer || buffer_length < 0)
		throw false;
	result.emplace((const char *) buffer, buffer_length);
#endif // __WINDOWS__

	return result;
}
