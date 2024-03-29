/***************************************************************************

	7zip.h

	Logic for handling 7-Zip (*.7z)

***************************************************************************/

#ifndef X7Z_H
#define X7Z_H

// Qt headers
#include <QIODevice>

// standard headers
#include <unordered_map>


// ======================> SevenZipFile

class SevenZipFile
{
public:
	SevenZipFile();
	SevenZipFile(const SevenZipFile &) = delete;
	SevenZipFile(SevenZipFile &&) = delete;
	~SevenZipFile();

	bool open(const QString &path);
	std::unique_ptr<QIODevice> get(const QString &fileName);
	std::unique_ptr<QIODevice> get(std::uint32_t crc32);

private:
	class Impl;

	std::unique_ptr<Impl>					m_impl;
	std::unordered_map<QString, int>		m_filesByName;
	std::unordered_map<std::uint32_t, int>	m_filesByCrc32;
	int										m_cursor;

	static QString normalizeFileName(const QString &fileName);
	std::unique_ptr<QIODevice> extractOrPopulate(
		std::optional<int> index,
		const std::optional<QString> &normalizedFileName,
		std::optional<std::uint32_t> crc32);
};


#endif // X7Z_H
