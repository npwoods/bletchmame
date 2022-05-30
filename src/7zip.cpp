/***************************************************************************

	7zip.cpp

	Logic for handling 7-Zip (*.7z)

***************************************************************************/

// bletchmame headers
#include "7zip.h"

// Qt headers
#include <QBuffer>
#include <QFile>

// liblzma headers
#include <lzma/7z.h>
#include <lzma/7zAlloc.h>
#include <lzma/7zFile.h>
#include <lzma/7zCrc.h>
#include <lzma/7zTypes.h>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class CrcTableGenerator
	{
	public:
		CrcTableGenerator()
		{
			CrcGenerateTable();
		}
	};
}

class SevenZipFile::Impl : private ISeekInStream
{
public:
	typedef std::unique_ptr<Impl> ptr;

	// ctor/dtor
	Impl();
	~Impl();

	// methods
	bool open(std::unique_ptr<QIODevice> &&stream);
	std::unique_ptr<QIODevice> extract(int index);
	int entryCount() const;
	QString entryName(int index) const;
	bool entryIsDirectory(int index) const;
	std::optional<std::uint32_t> entryCrc32(int index) const;

private:
	static const ISzAlloc			s_allocImpl;
	static const ISzAlloc			s_allocTempImpl;
	static const CrcTableGenerator	s_crcTableGenerator;

	std::unique_ptr<QIODevice>		m_stream;
	CSzArEx							m_db;
	CLookToRead2					m_lookToRead;
	Byte							m_lookToReadBuffer[4096];

	// ILookInStream implementation
	SRes doRead(void *buf, size_t *size);
	SRes doSeek(Int64 *pos, ESzSeek origin);

	// trampolines
	static Impl &getImpl(const ISeekInStream *seekInStream);
	static SRes readTrampoline(const ISeekInStream *p, void *buf, size_t *size);
	static SRes seekTrampoline(const ISeekInStream *p, Int64 *pos, ESzSeek origin);
};


//**************************************************************************
//  STATIC VARIABLES
//**************************************************************************

const ISzAlloc SevenZipFile::Impl::s_allocImpl = { SzAlloc, SzFree };
const ISzAlloc SevenZipFile::Impl::s_allocTempImpl = { SzAllocTemp, SzFreeTemp };
const CrcTableGenerator SevenZipFile::Impl::s_crcTableGenerator;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

SevenZipFile::SevenZipFile()
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

SevenZipFile::~SevenZipFile()
{
}


//-------------------------------------------------
//  open(const QString &path)
//-------------------------------------------------

bool SevenZipFile::open(const QString &path)
{
	// create the file
	std::unique_ptr<QFile> file = std::make_unique<QFile>(path);
	if (!file->open(QFile::ReadOnly))
		return false;

	// create the impl and initialize it
	m_impl = std::make_unique<Impl>();
	if (!m_impl->open(std::move(file)))
		return false;

	// prepare our file indexes
	int entryCount = m_impl->entryCount();
	m_filesByName.clear();
	m_filesByCrc32.clear();
	m_filesByName.reserve(entryCount);
	m_filesByCrc32.reserve(entryCount);

	// add all the entries
	for (int i = 0; i < m_impl->entryCount(); i++)
	{
		if (!m_impl->entryIsDirectory(i))
		{
			QString name = m_impl->entryName(i);
			m_filesByName.emplace(normalizeFileName(name), i);

			std::optional<std::uint32_t> crc32 = m_impl->entryCrc32(i);
			if (crc32.has_value())
				m_filesByCrc32.emplace(crc32.value(), i);
		}
	}

	// and we're done
	return true;
}


//-------------------------------------------------
//  get(const QString &)
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::get(const QString &fileName)
{
	QString normalizedFileName = normalizeFileName(fileName);
	auto iter = m_filesByName.find(normalizedFileName);
	return iter != m_filesByName.end()
		? m_impl->extract(iter->second)
		: std::unique_ptr<QIODevice>();
}


//-------------------------------------------------
//  get(std::uint32_t crc32)
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::get(std::uint32_t crc32)
{
	auto iter = m_filesByCrc32.find(crc32);
	return iter != m_filesByCrc32.end()
		? m_impl->extract(iter->second)
		: std::unique_ptr<QIODevice>();
}


//-------------------------------------------------
//  normalizeFileName
//-------------------------------------------------

QString SevenZipFile::normalizeFileName(const QString &fileName)
{
	return fileName.toUpper();
}


//-------------------------------------------------
//  Impl ctor
//-------------------------------------------------

SevenZipFile::Impl::Impl()
{
	// initialize archive
	SzArEx_Init(&m_db);

	// initialize CLookToRead2
	memset(&m_lookToRead, 0, sizeof(m_lookToRead));
	LookToRead2_CreateVTable(&m_lookToRead, false);
	LookToRead2_Init(&m_lookToRead);
	m_lookToRead.realStream = this;
	m_lookToRead.buf = m_lookToReadBuffer;
	m_lookToRead.bufSize = sizeof(m_lookToReadBuffer);

	// initalize ISeekInStream with trampolines
	Read = readTrampoline;
	Seek = seekTrampoline;
}


//-------------------------------------------------
//  Impl dtor
//-------------------------------------------------

SevenZipFile::Impl::~Impl()
{
	SzArEx_Free(&m_db, &s_allocImpl);
}


//-------------------------------------------------
//  Impl::open
//-------------------------------------------------

bool SevenZipFile::Impl::open(std::unique_ptr<QIODevice> &&stream)
{
	m_stream = std::move(stream);
	SRes res = SzArEx_Open(&m_db, &m_lookToRead.vt, &s_allocImpl, &s_allocTempImpl);
	return res == SZ_OK;
}


//-------------------------------------------------
//  Impl::extract
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::Impl::extract(int index)
{
	// perform the extraction
	UInt32 blockIndex = 0;
	Byte *buffer = nullptr;
	std::size_t bufferSize = 0;
	std::size_t offset = 0;
	std::size_t sizeProcessed = 0;
	SRes res = SzArEx_Extract(&m_db, &m_lookToRead.vt, index, &blockIndex, &buffer, &bufferSize, &offset, &sizeProcessed, &s_allocImpl, &s_allocTempImpl);
	if (res != SZ_OK)
		return { };

	// create a buffer to return
	std::unique_ptr<QBuffer> result = std::make_unique<QBuffer>();
	QByteArray &byteArray = result->buffer();

	// resize the returned buffer appropriately
	int intSizeProcessed = (int)sizeProcessed;
	if (sizeProcessed != intSizeProcessed)
		return { };
	byteArray.resize(intSizeProcessed);

	// copy the buffer and return
	memcpy(byteArray.data(), buffer + offset, sizeProcessed);
	return result;
}


//-------------------------------------------------
//  Impl::entryCount
//-------------------------------------------------

int SevenZipFile::Impl::entryCount() const
{
	return m_db.NumFiles;
}


//-------------------------------------------------
//  Impl::entryName
//-------------------------------------------------

QString SevenZipFile::Impl::entryName(int index) const
{
	std::size_t nameLen = SzArEx_GetFileNameUtf16(&m_db, index, nullptr);

	QString name;
	name.resize(nameLen);
	SzArEx_GetFileNameUtf16(&m_db, index, (UInt16 *)&name[0]);

	// get rid of trailing NUL, if present
	if (nameLen > 0 && name[nameLen - 1] == QChar('\0'))
		name.resize(nameLen - 1);

	return name;
}


//-------------------------------------------------
//  Impl::entryIsDirectory
//-------------------------------------------------

bool SevenZipFile::Impl::entryIsDirectory(int index) const
{
	return SzArEx_IsDir(&m_db, index);
}


//-------------------------------------------------
//  Impl::entryCrc32
//-------------------------------------------------

std::optional<std::uint32_t> SevenZipFile::Impl::entryCrc32(int index) const
{
	std::optional<std::uint32_t> result;
	if (SzBitArray_Check(m_db.CRCs.Defs, index))
		result = m_db.CRCs.Vals[index];
	return result;
}


//-------------------------------------------------
//  Impl::doRead
//-------------------------------------------------

SRes SevenZipFile::Impl::doRead(void *buf, size_t *size)
{
	qint64 byteCount = *size;
	*size = m_stream->read(reinterpret_cast<char *>(buf), byteCount);
	return *size == byteCount ? SZ_OK : SZ_ERROR_READ;
}


//-------------------------------------------------
//  Impl::doSeek
//-------------------------------------------------

SRes SevenZipFile::Impl::doSeek(Int64 *pos, ESzSeek origin)
{
	// determine the absolute position
	qint64 qpos;
	switch (origin)
	{
	case SZ_SEEK_SET:
		qpos = *pos;
		break;
	case SZ_SEEK_CUR:
		qpos = *pos + m_stream->pos();
		break;
	case SZ_SEEK_END:
		qpos = *pos + m_stream->size();
		break;
	default:
		throw false;
	}

	// seek!
	if (!m_stream->seek(qpos))
		return SZ_ERROR_UNSUPPORTED;

	// and we've succeeded
	*pos = m_stream->pos();
	return SZ_OK;
}


//**************************************************************************
//  TRAMPOLINES
//**************************************************************************

//-------------------------------------------------
//  Impl::getImpl
//-------------------------------------------------

SevenZipFile::Impl &SevenZipFile::Impl::getImpl(const ISeekInStream *seekInStream)
{
	ISeekInStream *nonConstSeekInStream = const_cast<ISeekInStream *>(seekInStream);
	return *reinterpret_cast<Impl *>(nonConstSeekInStream);
}


//-------------------------------------------------
//  Impl::readTrampoline
//-------------------------------------------------

SRes SevenZipFile::Impl::readTrampoline(const ISeekInStream *p, void *buf, size_t *size)
{
	return getImpl(p).doRead(buf, size);
}


//-------------------------------------------------
//  Impl::seekTrampoline
//-------------------------------------------------

SRes SevenZipFile::Impl::seekTrampoline(const ISeekInStream *p, Int64 *pos, ESzSeek origin)
{
	return getImpl(p).doSeek(pos, origin);
}
