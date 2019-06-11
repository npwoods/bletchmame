/***************************************************************************

    validity.cpp

    Miscellaneous utility code

***************************************************************************/

#include <list>

#include "validity.h"
#include "utility.h"
#include "xmlparser.h"


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
