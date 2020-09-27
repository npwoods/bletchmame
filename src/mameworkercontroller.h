/***************************************************************************

    mameworkercontroller.h

    Logic for issuing worker_ui commands and receiving responses

***************************************************************************/

#ifndef MAMEWORKERCONTROLLER_H
#define MAMEWORKERCONTROLLER_H

#include <QString>
#include <optional>

#include "status.h"

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE


// ======================> MameWorkerController

class MameWorkerController
{
public:
	struct Response
	{
		enum class Type
		{
			Unknown,
			Ok,
			EndOfFile,
			Error
		};

		Type							m_type;
		QString							m_text;
		std::optional<status::update>	m_update;
	};

	enum class ChatterType
	{
		Command,
		GoodResponse,
		ErrorResponse
	};

	// ctor
	MameWorkerController(QProcess &process, std::function<void(ChatterType, const QString &)> &&chatterCallback);

	// methods
	Response receiveResponse();
	void issueCommand(const QString &command);

private:
    QProcess &											m_process;
	std::function<void(ChatterType, const QString &)>	m_chatterCallback;

	// private methods
	status::update readStatus();
	QString reallyReadLineFromProcess();
	void callChatterCallback(ChatterType chatterType, const QString &text) const;
};


#endif // MAMEWORKERCONTROLLER_H
