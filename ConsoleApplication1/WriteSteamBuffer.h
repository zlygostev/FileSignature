#pragma once

#include <string>
#include <vector>
#include <list>
#include <functional>
#include <atomic>
#include <mutex>
#include <sstream>
#include <cstdio>
#include <iostream>
#include "easylogging++.h"
#include "IWriteStream.h"
#include "CommonStreamBuffer.h"

using namespace std;

namespace transformation_stream
{
// An asynchronous binary output stream.
// It is implemented for a sequential streaming of a user content to a file by blocks with minimal copying operations.
// As a performance optimization, there is an background thread. 
// It as sequentially write of user's content from an internal buffer to the file.
class WriteStream : public IWriteStream
{
public:
	// Input params:
	// file - a name of a file to write
	// maxBufferSize - maximal size in bytes of an internal file cache for background dumping to file
	WriteStream(const std::string& file, size_t maxBufferSize, size_t ioBlockSize) :
		m_maxBufferSize(maxBufferSize),
		m_bufferSize(0),
		m_totalWritten(0),
		m_notFlushed(0),
		m_ioBlockSize(ioBlockSize),
		m_fileName(file),
		m_file(nullptr),
		m_isEOF(false),
		m_needStop(false),
		m_errno(0)
	{
		if (m_maxBufferSize == 1)
		{
			throw std::invalid_argument("max Buffer size should have positive value.");
		}
		// open file
#ifdef _WIN32
		auto myErrno = fopen_s(&m_file, file.c_str(), "wb");
		if (myErrno)
		{
#else
		m_file = fopen(file.c_str(), "wb");
		if (!m_file)
		{
			const int myErrno = errno;
#endif
			stringstream ss;
			ss << "Can't open file for write " << file << ". ";
			LOG(ERROR) << ss.str() << ". Errno " << myErrno;
			throwOnFileError(ss.str(), myErrno);
		}
		// run background thread of file read to buffer
		m_backgroundWrite = make_unique<thread>(std::bind(&WriteStream::backgroundWrittingToFile, this));
	}


	virtual ~WriteStream()
	{
		if (!m_isEOF)
		{
			cancel();
		}
		LOG(INFO) << "Total bytes write " << m_totalWritten;
		m_writeEventsCV.notify_one();
		m_readEventsCV.notify_one();
		m_backgroundWrite->join();
		LOG(INFO) << "Background stream of file write to disk is closed";
		fclose(m_file);
		LOG(INFO) << "The result file is closed";
		if (!m_isEOF || m_errno)
		{
			LOG(WARNING) << "The result file is removed";
			remove(m_fileName.c_str());
		}

	}

	void close() override
	{
		if (m_errno)
		{
			throwOnFileError("Can't close file. Background write has an error: ", m_errno);
		}

		m_isEOF = true;
		m_writeEventsCV.notify_one();
		//TODO
	}

	void cancel() override
	{
		m_needStop = true;
		m_errno = EINTR;
		m_writeEventsCV.notify_one();
		m_readEventsCV.notify_one();
	}

	bool isFreeSpaceEnoghtForWrite(size_t chunkSize)
	{
		//Potentially it could be negative right part so type should be signed
		return static_cast<int64_t>(chunkSize) <= static_cast<int64_t>(m_maxBufferSize - m_bufferSize);
	}

	void putAsync(BufferPTR chunk) override
	{
		//TIMED_FUNC(OSPutTimerObj);
		if (!chunk)
		{
			LOG(WARNING) << "empty chunk is come for write";
			return;
		}

		if (m_errno)
		{
			LOG(ERROR) << "Can't write to file. Errno " << m_errno;
			throwOnFileError("Can't write to file: ", m_errno);
		}

		if (m_isEOF)
		{
			std::string msg = "file is already closed";
			LOG(ERROR) << msg;
			throw invalid_argument(msg);
		}

		if (m_needStop)
		{
			std::string msg = "file is already canceled";
			LOG(ERROR) << msg;
			throw invalid_argument(msg);
		}


		const auto chunkSize = chunk->size();
		if (chunkSize > m_maxBufferSize)
		{
			stringstream msg_stream;
			msg_stream << "Stream error: Attempt to write asynchroniusly chunk of data larger then maximum buffer size (" << m_maxBufferSize << ")";
			LOG(ERROR) << msg_stream.str();
			throw invalid_argument(msg_stream.str());
		}

		//static_cast for cases if value of difference negat
		if (!isFreeSpaceEnoghtForWrite(chunkSize))
		{
			LOG(DEBUG) << "wait: buffer is full. Size: " << m_bufferSize;
			waitIfBufferFull(chunkSize);
			LOG(DEBUG) << "stop waiting buffer. Size: " << m_bufferSize;
		}
		m_bufferSize += chunkSize;
		m_totalWritten += chunkSize;
		unique_lock<decltype(m_bufferMutex)> lock(m_bufferMutex);
		m_buffers.push_back(std::move(chunk));
		lock.unlock();
		LOG(DEBUG) << "New data is written on buffer. Size: " << m_bufferSize << ". Total written " << m_totalWritten;
		m_writeEventsCV.notify_one();

	}

protected:
	bool isNeedUserThreadWakeup(size_t chunkSize)
	{
		return (chunkSize <= m_maxBufferSize - m_bufferSize) || m_isEOF || m_needStop;
	}

	void waitIfBufferFull(size_t chunkSize)
	{
		while ( !isFreeSpaceEnoghtForWrite(chunkSize) && !m_isEOF && !m_needStop)
		{
			LOG(DEBUG) << "Start waiting buffer for free space to write to. Buffers size " << m_bufferSize << ", need " << chunkSize;
			unique_lock<decltype(m_readEventsCVMutex)> lock(m_readEventsCVMutex);
			if (!m_readEventsCV.wait_for(lock, chrono::seconds(15), std::bind(&WriteStream::isNeedUserThreadWakeup, this, chunkSize)))
			{
				LOG(WARNING) << "Timeout on free space in buffer waiting. Buffers size " << m_bufferSize << ", need " << chunkSize;
			}
		}
	}

	bool isNeedToStopWrite()
	{
		return  (m_isEOF && m_bufferSize == 0) 
				|| m_needStop || m_errno;
	}

	bool needBackgroundThreadWakeup()
	{
		return  static_cast<int64_t>(m_bufferSize) > 0 || m_isEOF || m_needStop;
	}

	// wait user's inputs to buffer
	void waitNewData()
	{
		//TIMED_FUNC(OSTimerWaitWriteEvents);
		unique_lock<decltype(m_writeEventsCVMutex)> lock(m_writeEventsCVMutex);
		if (!m_writeEventsCV.wait_for(lock, chrono::milliseconds(15000), std::bind(&WriteStream::needBackgroundThreadWakeup, this)))
		{
			LOG(WARNING) << "Timeout on background write  waiting. Buffers " << m_bufferSize ;
		}
	}
	void flush() 
	{
		if (fflush(m_file) != 0)
		{
			m_errno = errno;
			if (m_errno)
			{
				LOG(ERROR) << "File flush error " << m_errno << " on or before bytes " << m_totalWritten;
				m_isEOF = true;
			}
		}
		m_notFlushed = 0;

	}

	//Write a content from buffer to the file
	void backgroundWrittingToFile()
	{
		try 
		{
			//TIMED_FUNC(OSTimerObj1);
			while (!isNeedToStopWrite())
			{
				//TIMED_SCOPE(timerBlkObj1, "bg-Wr2File");
				unique_lock<decltype(m_bufferMutex)> lock(m_bufferMutex);
				if (m_buffers.empty())
				{
					lock.unlock();
					if(m_notFlushed)
					{
						flush();
					}
					LOG(DEBUG) << "Buffer is empty. Stop write to file. Waiting new data";
					waitNewData();
					LOG(DEBUG) << "Stop waiting new data in buffer. Buffer size " << m_bufferSize;
					continue;
				}

				BufferPTR ptr = std::move(m_buffers.front());
				const size_t bufferSize = ptr->size();
				m_notFlushed += bufferSize;
				m_bufferSize -= bufferSize;
				bool needFlush = (m_notFlushed >= m_ioBlockSize) ? true : false;
				m_buffers.pop_front();
				lock.unlock();
				m_readEventsCV.notify_one();
				size_t writeCount = fwrite(&(*ptr)[0], 1/*sizeof(char_type)*/, bufferSize, m_file);
				if (writeCount != bufferSize)
				{
					m_errno = errno;
					//TODO: Return buffer back on errno EINTR
					//Stop work
					m_isEOF = true;
					break;
				}
				if (needFlush)
				{
					flush();
				}
			}

		}
		catch (const std::exception& ex)
		{
			LOG(ERROR) << "Exception on background write to file: " << ex.what();
			m_errno = EINTR;
			m_isEOF = true;
		}
	}

	const size_t m_maxBufferSize;//in bytes
	std::atomic<size_t> m_bufferSize;//in bytes. Atomic because in some cases uses without mutex. It's a bit faster
	std::atomic<size_t> m_totalWritten; //in bytes. just for logging

	//Events of user's write to buffer
	mutex m_writeEventsCVMutex;
	condition_variable m_writeEventsCV;

	//A buffer with a prepared chunks of data for sequentual write to file
	ListOfBuffers m_buffers;
	mutex m_bufferMutex;
	unique_ptr<std::thread> m_backgroundWrite;
	size_t m_notFlushed; // in bytes. It indicates how much bytes were written to file without flush operation  
	const size_t m_ioBlockSize; // Optimal size of block to flash on disk


	//Events of background consumption data from buffer
	mutex m_readEventsCVMutex;
	condition_variable m_readEventsCV;

	const std::string m_fileName;
	FILE *m_file;

	// State of backgroundly processing file stream
	atomic<bool> m_isEOF;
	atomic<bool> m_needStop;
	atomic<int> m_errno;
};
};//end of namespace