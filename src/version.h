/***************************************************************************

	version.h

	Version information (lots of Windows stuff, but some stuff used
	generally)

***************************************************************************/

#ifndef VERSION_H
#define VERSION_H

//**************************************************************************
//  GENERATED VERSION INFORMATION
//**************************************************************************

// "version.gen.h" is generated at build time (and of course is expected to
// be present in anything released), but we want to tolerate its absence
#ifdef HAS_VERSION_GEN_H
#include "version.gen.h"
#endif // HAS_VERSION_GEN_H

#ifndef BLETCHMAME_VERSION_STRING
#define BLETCHMAME_VERSION_STRING ""
#endif // BLETCHMAME_VERSION_STRING


//**************************************************************************
//  WINDOWS RC SPECIFIC
//**************************************************************************

#ifdef RC_INVOKED
#define WINRCVER_COMPANYNAME_STR         "BletchMAME"
#define WINRCVER_FILEDESCRIPTION_STR     "BletchMAME"
#define WINRCVER_INTERNALNAME_STR        "BletchMAME"
#define WINRCVER_PRODUCTNAME_STR         "BletchMAME"

#ifdef BLETCHMAME_VERSION_COMMASEP
#define WINRCVER_FILEVERSION			BLETCHMAME_VERSION_COMMASEP
#define WINRCVER_PRODUCTVERSION			BLETCHMAME_VERSION_COMMASEP
#else
#define WINRCVER_FILEVERSION
#define WINRCVER_PRODUCTVERSION
#endif

#define WINRCVER_FILEVERSION_STR		BLETCHMAME_VERSION_STRING "\0"
#define WINRCVER_PRODUCTVERSION_STR		BLETCHMAME_VERSION_STRING "\0"

#endif // RC_INVOKED

#endif // VERSION_H
