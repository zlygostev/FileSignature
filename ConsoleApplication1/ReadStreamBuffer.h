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
	ReadStream(const std::string& file, size_t maxBufferSize, size_t blockSize) :
		m_maxBufferSize(maxBufferSize),
		m_bufferSize(0),
		m_totalRead(0),
		m_IOBlockSize(blockSize),
		m_file(nullptr),
		m_isEOF(false),
		m_needStop(false),
		m_errno(0)

	{
		LOG(INFO) << "Creating ReadStream";
		if (m_maxBufferSize < 2 * m_IOBlockSize)
		{
			std::string err = "Maximal buffer size should be larger at list twice of IO block operation. "
				"It is a performance optimization of the parallel work";
			LOG(ERROR) << err;
			throw std::invalid_argument(err);
		}
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

	BufferPTR get() override
	{
		//TIMED_FUNC(ISGetTimerObj);
		do
		{
			//TIMED_SCOPE(ISGetTimerBlkObj, "user get");
			unique_lock<decltype(m_bufferMutex)> lock(m_bufferMutex);
			if (!m_buffers.empty()) {
				BufferPTR ptr = std::move(m_buffers.front());
				const auto bufSize = ptr->size();
				m_bufferSize -= bufSize;
				m_buffers.pop_front(); 
				lock.unlock();
				LOG(DEBUG) << "Extracted chunk "<< bufSize <<" B by user. Buffer size " << m_bufferSize;
				m_ReadEventsCV.notify_one();
				return std::move(ptr);
			}
			if (m_isEOF)
			{
				if (m_errno)
				{
					std::string err = "Read file error. ";
					LOG(ERROR) << err << "Errno " << m_errno;
					throwOnFileError(err, m_errno);
				}
				return std::move(BufferPTR(nullptr));
			}
			lock.unlock();
			waitNewData();
		} while (!(m_isEOF && m_bufferSize == 0) );
		return std::move(BufferPTR(nullptr));
	}

	bool isEOF() override
	{
		return m_isEOF && (m_bufferSize == 0);
	}

	///Stop background read of file
	void stop() override
	{
		if (m_needStop)
			return;

		m_needStop = true;
		m_errno = EINTR;
		m_isEOF = true;
		m_WriteEventsCV.notify_one();
		m_ReadEventsCV.notify_one();
		m_backgroundRead->join();
	}
private:
	bool needReadThreadWakeup()
	{
		return m_isEOF || m_bufferSize>0;
	}

	void waitNewData()
	{
		//TIMED_FUNC(ISWaitWriteEventsTimerBlkObj);
		unique_lock<decltype(m_WriteEventsCVMutex)> lock(m_WriteEventsCVMutex);
		if (!m_WriteEventsCV.wait_for(lock, chrono::milliseconds(15000), std::bind(&ReadStream::needReadThreadWakeup, this)))
		{
			LOG(WARNING) << "Timeout on background write waiting. Buffers "  << m_bufferSize;
		}
	}

	bool isFreeSpaceEnoghtForWrite()
	{
		return static_cast<int64_t>(m_IOBlockSize) <= static_cast<int64_t>(m_maxBufferSize - m_bufferSize);
	}

	bool needWriteThreadWakeup()
	{
		return m_isEOF || m_needStop || isFreeSpaceEnoghtForWrite();
	}

	void waitIfBufferFull()
	{
		if (!isFreeSpaceEnoghtForWrite())
		{
			LOG(DEBUG) << "start waiting buffer free space. Buffer size: " << m_bufferSize;
			unique_lock<decltype(m_WriteEventsCVMutex)> lock(m_WriteEventsCVMutex);
			if (!m_ReadEventsCV.wait_for(lock, chrono::milliseconds(15000), std::bind(&ReadStream::needWriteThreadWakeup, this)))
			{
				LOG(WARNING) << "Timeout on wait of client's read. Buffers " << m_bufferSize;
			}
			LOG(DEBUG) << "stop waiting buffer. Buffer size: " << m_bufferSize;
		}

	}

	///Write to  buffer a content from the file
	void backgroundWrittingToBuffer() 
	{
		try
		{
			//TIMED_FUNC(ISTimerObj);
			while (!m_isEOF && !m_needStop)
			{
				LOG(DEBUG) << "Loop of reading file";
				waitIfBufferFull();

				if (m_needStop)
				{
					return;
				}
				//check if enoght space in the buffer for new portion of file data 
				if (m_IOBlockSize > m_maxBufferSize - m_bufferSize)
				{
					continue;
				}

				bool isEndOfFile = false;
				auto bufferPtr = make_unique<BufferT>(m_IOBlockSize);
				LOG(DEBUG) << "Start Reading new block from file";

#ifdef _WIN32
				const size_t readCount = fread_s(&(*bufferPtr)[0], bufferPtr->size(), 1/*sizeof(char_type)*/, m_IOBlockSize, m_file);
#else
				const size_t readCount = fread(&(*bufferPtr)[0], 1/*sizeof(char_type)*/, m_minIOSize, m_file);
#endif
				//PERFORMANCE_CHECKPOINT_WITH_ID(ISTimerBlkObj, "readed");
				if (readCount != m_IOBlockSize)
				{
					const int myErrno = errno;
					bufferPtr->resize(readCount);
					if (feof(m_file))
					{
						LOG(INFO) << "The file has been read till the end successfully";
						isEndOfFile = true;
					}
					else
					{
						if (myErrno != EINTR)
						{
							//Stop work
							std::string err = "Read file error. ";
							m_errno = myErrno;// Save it as member for throw it to user
							LOG(ERROR) << err << "Errno " << m_errno;
							m_isEOF = true;
							break;
						}
						// Continue read on EINTR
						clearerr(m_file);
						if (readCount == 0) //IF no new data - no need to add something in buffer
							continue;
					}
				}
				unique_lock<decltype(m_bufferMutex)> lock(m_bufferMutex);
				const auto bufSize = bufferPtr->size();
				m_buffers.push_back(std::move(bufferPtr));
				lock.unlock();

				m_bufferSize += bufSize;
				if (isEndOfFile)
				{
					m_isEOF = isEndOfFile;
				}				
				m_totalRead += bufSize;

				LOG(DEBUG) << "Read from file new block of " << bufSize << " (B) to buffer. Buffer size "<< m_bufferSize  << " Total " << m_totalRead;
				m_WriteEventsCV.notify_one();
			}
		}
		catch (const std::exception& ex)
		{
			LOG(ERROR) << "Exception on background write to file: " << ex.what();
			m_errno = EINTR;
			m_isEOF = true;
		}
	}


private:
	const size_t m_maxBufferSize;//in bytes.
	std::atomic<size_t> m_bufferSize;//in bytes. Atomic because in some cases uses without mutex. It's a bit faster
	const size_t m_IOBlockSize;//in bytes. Minimal chunk to read from disk.
	size_t m_totalRead; //in bytes. Just for logs.

	//Events of read from buffer
	mutex m_ReadEventsCVMutex;
	condition_variable m_ReadEventsCV;

	// buffer with prepared chunks for user
	ListOfBuffers m_buffers;
	mutex m_bufferMutex;

	unique_ptr<std::thread> m_backgroundRead;

	//Events of background write to buffer from file
	mutex m_WriteEventsCVMutex;
	condition_variable m_WriteEventsCV;

	FILE *m_file;
	// State of backgroundly processing file stream
	atomic<bool> m_isEOF;
	atomic<bool> m_needStop;
	atomic<int> m_errno; // Not 0 if an error is occured
};

}// end of namespace stream_buffer