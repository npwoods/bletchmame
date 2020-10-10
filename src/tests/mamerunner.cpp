/***************************************************************************

    mamerunner.cpp

    Testing infrastructure

***************************************************************************/

#include <iostream>
#include <stdexcept>
#include <QDir>
#include <QProcess>
#include <QThread>

#include "mamerunner.h"
#include "mameversion.h"
#include "mameworkercontroller.h"
#include "xmlparser.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define HAS_COLOR   1


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
//  getScriptRequiredMameVersion
//-------------------------------------------------

static std::optional<MameVersion> getScriptRequiredMameVersion(const QString &scriptFileName)
{
    std::optional<MameVersion> result;
    XmlParser xml;
    xml.onElementBegin({ "script" }, [&result](const XmlParser::Attributes &attributes)
    {
        QString requiredVersionString;
        if (attributes.get("requiredVersion", requiredVersionString))
            result = MameVersion(requiredVersionString);
    });

    if (!xml.parse(scriptFileName))
        throw std::logic_error(QString("Could not load script '%1'").arg(scriptFileName).toLocal8Bit().constData());
    return result;
}


//-------------------------------------------------
//  getMameVersion
//-------------------------------------------------

static MameVersion getMameVersion(const QString &program)
{
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start(program, QStringList() << "-version");
    process.waitForFinished();
    QByteArray output = process.readAllStandardOutput();
    QString versionString = QString::fromUtf8(output);
    return MameVersion(versionString);
}


//-------------------------------------------------
//  internalRunAndExcerciseMame
//-------------------------------------------------

static void internalRunAndExcerciseMame(const QString &scriptFileName, const QString &program, const QStringList &arguments)
{
    // check to see if the program file exists
    QFileInfo fileInfo(program);
    if (!fileInfo.isFile())
        throw std::logic_error(QString("MAME program '%1' not found").arg(program).toLocal8Bit().constData());

    // identify the script we're running
    std::cout << QString("Running script: %1").arg(scriptFileName).toStdString() << std::endl;

    // version check
    std::optional<MameVersion> requiredMameVersion = getScriptRequiredMameVersion(scriptFileName);
    if (requiredMameVersion)
    {
        MameVersion mameVersion = getMameVersion(program);
        if (!mameVersion.isAtLeast(*requiredMameVersion))
        {
            std::cout << QString("Script requires MAME %1 (this is MAME %2)").arg(requiredMameVersion->toString(), mameVersion.toString()).toStdString() << std::endl;
            return;
        }
    }

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

    // get ready to parse
    XmlParser xml;
    xml.onElementEnd({ "script", "command" }, [&controller](QString &&text)
    {
        // normalize the text
        text = text.trimmed() + "\n";

        // issue the command
        issueCommandAndReceiveResponse(controller, text.toLocal8Bit());
    });
    xml.onElementBegin({ "script", "sleep" }, [&controller](const XmlParser::Attributes &attributes)
    {
        float seconds = 0.0;
        attributes.get("seconds", seconds);
        QThread::msleep((unsigned long)(seconds * 1000));
    });

    // load the script
    if (!xml.parse(scriptFileName))
        throw std::logic_error(QString("Could not load script '%1'").arg(scriptFileName).toLocal8Bit().constData());

    // wait for exit
    if (!process.waitForFinished())
        throw std::logic_error("waitForFinished() returned false");
}


//-------------------------------------------------
//  runAndExcerciseMame
//-------------------------------------------------

int runAndExcerciseMame(int argc, char *argv[])
{
    int currentArg = 0;

    // identify the scripts
    QFileInfoList scripts;
    QString scriptOrDirectory = argv[currentArg++];
    QFileInfo fi(scriptOrDirectory);
    if (fi.isFile())
    {
        scripts << std::move(fi);
    }
    else if (fi.isDir())
    {
        QDir scriptDir(scriptOrDirectory);
        scripts = scriptDir.entryInfoList(QStringList() << "*.xml", QDir::Files);
    }
    if (scripts.isEmpty())
        throw std::logic_error("No scripts");

    // identify the program
    QString program = argv[currentArg++];

    // identify the arguments
    QStringList arguments;
    while(currentArg < argc)
        arguments << argv[currentArg++];

    int result;
    try
    {
        for (const QFileInfo &script : scripts)
        {
            internalRunAndExcerciseMame(script.absoluteFilePath(), program, arguments);
        }
        result = 0;
    }
    catch (std::exception &ex)
    {
        std::cout << "EXCEPTION: " << ex.what() << std::endl;
        result = 1;
    }
    return result;
}
