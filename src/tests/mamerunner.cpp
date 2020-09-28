/***************************************************************************

    mamerunner.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include <stdexcept>
#include <QProcess>
#include <QThread>
#include "mamerunner.h"
#include "mameworkercontroller.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define HAS_COLOR   0


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  getAnsiColorCodeForChatterType
//-------------------------------------------------

static const char *getAnsiColorCodeForChatterType(MameWorkerController::ChatterType chatterType)
{
    const char *result;
    switch (chatterType)
    {
    case MameWorkerController::ChatterType::Command:
        result = "\x1B[36m";
        break;
    case MameWorkerController::ChatterType::GoodResponse:
        result = "\x1B[92m";
        break;
    case MameWorkerController::ChatterType::ErrorResponse:
        result = "\x1B[91m";
        break;
    default:
        throw false;
    }
    return result;
}


//-------------------------------------------------
//  chatter
//-------------------------------------------------

static void chatter(MameWorkerController::ChatterType chatterType, const QString &text)
{
    if (HAS_COLOR)
        std::cout << getAnsiColorCodeForChatterType(chatterType);
    std::cout << text.trimmed().toLocal8Bit().constData() << std::endl;
}


//-------------------------------------------------
//  receiveResponseEnsureSuccess
//-------------------------------------------------

static MameWorkerController::Response issueCommandAndReceiveResponse(MameWorkerController &controller, const char *command)
{
    // issue the command
    controller.issueCommand(command);

    // and get a response
    MameWorkerController::Response response = controller.receiveResponse();
    if (response.m_type != MameWorkerController::Response::Type::Ok)
        throw std::logic_error(QString("Received invalid response from MAME: %1").arg(response.m_text).toLocal8Bit());
    return response;
}


//-------------------------------------------------
//  internalRunAndExcerciseMame
//-------------------------------------------------

static void internalRunAndExcerciseMame(const QString &program, const QStringList &arguments)
{
    // check to see if the program file exists
    QFileInfo fileInfo(program);
    if (!fileInfo.isFile())
        throw std::logic_error(QString("MAME program '%1' not found").arg(program).toLocal8Bit().constData());

    // start the process
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start(program, arguments);

    // report the process starting
    std::cout << QString("Started MAME: %1 %2").arg(program, arguments.join(" ")).toStdString() << std::endl;

    // start controlling MAME
    MameWorkerController controller(process, chatter);

    // read the initial response
    MameWorkerController::Response response = controller.receiveResponse();
    if (response.m_type != MameWorkerController::Response::Type::Ok)
    {
        QString errorMessage = !response.m_text.isEmpty()
            ? std::move(response.m_text)
            : controller.scrapeMameStartupError();
        throw std::logic_error(errorMessage.toLocal8Bit());
    }

    // turn off throttling
    issueCommandAndReceiveResponse(controller, "THROTTLED 0\n");

    // resume!
    issueCommandAndReceiveResponse(controller, "RESUME\n");

    // sleep and ping
    QThread::sleep(5);
    issueCommandAndReceiveResponse(controller, "PING\n");

    // sleep and exit
    QThread::sleep(5);
    issueCommandAndReceiveResponse(controller, "EXIT\n");

    // wait for exit
    if (!process.waitForFinished())
        throw std::logic_error("waitForFinished() returned false");
}


//-------------------------------------------------
//  runAndExcerciseMame
//-------------------------------------------------

int runAndExcerciseMame(int argc, char *argv[])
{
    // identify the program
    QString program = argv[0];

    // identify the arguments
    QStringList arguments;
    for (int i = 1; i < argc; i++)
        arguments << argv[i];

    int result;
    try
    {
        internalRunAndExcerciseMame(program, arguments);
        result = 0;
    }
    catch (std::exception &ex)
    {
        std::cout << "EXCEPTION: " << ex.what() << std::endl;
        result = 1;
    }
    return result;
}
