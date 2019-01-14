#pragma once

//#pragma warning(disable : 4996)
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <mutex>
#include <atomic>
#include <ios>
#include <functional>
#include <sstream>
#include <string.h>
#include <functional>
#include "easylogging++.h"

#include "CommonStreamBuffer.h"
#include "IReadStream.h"
#include "IQueue.h"
#include "MemBlocksPool.h"

using namespace std;
namespace transformation_stream
{

// An asynchronous binary input stream.
// It is implemented for a sequential streaming of a file's data by blocks with minimal copying operations.
// As a performance optimization, there is an background thread. It sequentially reads file's content to an internal buffer. 
class ReadStream: public IReadStream
{
public:
	// Input params:
	// file - name of file to read from
	// maxBufferSize - Maximal size in bytes of an internal file cache
	// blockSize - DataBlock size in bytes. It's a block size for communication with user and with disk
	ReadStream(const std::string& file, IStreamQueue& queue, MemBlocksPool& memPool, size_t blockSize) :
		m_IOBlockSize(blockSize),
		m_queue(queue),
		m_memPool(memPool),
		m_file(nullptr),
		m_isEOF(false),
		m_needStop(false)
	{
		LOG(INFO) << "Creating ReadStream";
		// open file
#ifdef _WIN32
		auto myErrno = fopen_s(&m_file, file.c_str(), "rb");
		if (myErrno)
		{
#else
		m_file = fopen(file.c_str(), "rb");
		if (!m_file)
		{
			const int myErrno = errno;
#endif
			stringstream ss;
			ss << "Can't open file " << file <<". ";
			LOG(ERROR) << ss.str() << "Errno " << myErrno;
			throwOnFileError(ss.str(), myErrno);
		}
		// run background thread of file read to buffer
		m_backgroundRead = make_unique<thread>(std::bind(&ReadStream::backgroundWrittingToBuffer, this));
	}

	virtual ~ReadStream() 
	{
		stop();
		fclose(m_file);
	}

	bool isEOF() override
	{
		return m_isEOF;
	}

	///Stop background read of file
	void stop() override
	{
		if (m_needStop)
			return;

		m_needStop = true;
		finishRead();
		m_backgroundRead->join();
	}
private:

	void finishRead()
	{
		m_isEOF = true;
		m_queue.stopInputStream();
	}

	///Write to  buffer a content from the file
	void backgroundWrittingToBuffer() 
	{
		size_t totalRead=0; //in bytes. Just for logs.
		int myErrno = 0;
		try
		{
			while (!m_isEOF && !m_needStop)
			{
				if (m_needStop)
				{
					return;
				}

				bool isEndOfFile = false;
				auto bufferPtr = std::move(m_memPool.get(m_IOBlockSize));
				LOG(DEBUG) << "Start Reading new block from file";

#ifdef _WIN32
				const size_t readCount = fread_s(&(*bufferPtr)[0], bufferPtr->size(), 1/*sizeof(char_type)*/, m_IOBlockSize, m_file);
#else
				const size_t readCount = fread(&(*bufferPtr)[0], 1/*sizeof(char_type)*/, m_minIOSize, m_file);
#endif
				if (readCount != m_IOBlockSize)
				{
					myErrno = errno;
					bufferPtr->resize(readCount);
					if (feof(m_file))
					{
						isEndOfFile = true;
					}
					else
					{
						if (myErrno != EINTR)
						{
							//Stop work
							m_queue.pushError(myErrno, "Read file error. ");
							finishRead();
							break;
						}
						// Continue read on EINTR
						clearerr(m_file);
					}
				}

				const auto bufSize = bufferPtr->size();
				if (bufSize != 0)
				{
					m_queue.push(std::move(bufferPtr));
					totalRead += bufSize;
				}

				if (isEndOfFile)
				{
					LOG(INFO) << "The file has been read till the end successfully";
					finishRead();
				}				

				LOG(DEBUG) << "The block of " << bufSize 
					<< " (B) has moved to queue. Total read " << totalRead;
			}
		}
		catch (const std::exception& ex)
		{
			stringstream ss;
			ss << "Exception on background read from file: " << ex.what();
			LOG(ERROR) << ss.str();
			m_queue.pushError(EINTR, ss.str());
			m_isEOF = true;
		}
		LOG(DEBUG) << "File has read till the end. Size " << totalRead << 
			"B. EOF=" << m_isEOF << "errno=" << myErrno;
	}


private:
	const size_t m_IOBlockSize;//in bytes. Minimal chunk to read from disk.

	unique_ptr<std::thread> m_backgroundRead;

	IStreamQueue& m_queue;
	MemBlocksPool& m_memPool;
	FILE *m_file;
	// State of backgroundly processing file stream
	atomic<bool> m_isEOF;
	atomic<bool> m_needStop;
};

}// end of namespace stream_buffer