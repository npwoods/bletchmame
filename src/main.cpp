/***************************************************************************

	main.cpp

	Main BletchMAME window

***************************************************************************/

#include "mainwindow.h"
#include "perfprofiler.h"
#include "version.h"

#include <QApplication>


//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
	// prepare the application; we can't do anything until this is done
	QApplication a(argc, argv);

	// profile this
	PerformanceProfiler perfProfiler("main.profiledata.txt");
	ProfilerScope prof(CURRENT_FUNCTION);

	// set the version string, if we have one
	if (strlen(BLETCHMAME_VERSION_STRING) > 0)
		QCoreApplication::setApplicationVersion(BLETCHMAME_VERSION_STRING);

	// run the application
	MainWindow w;
	w.show();
	return a.exec();
}
