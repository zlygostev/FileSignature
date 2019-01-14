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
#include "IQueue.h"
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
	// queue - a source of input stream
	// ioBlockSize - size in bytes of block for disk io communication
	WriteStream(const std::string& file, IStreamQueue& queue, size_t ioBlockSize) :
		m_ioBlockSize(ioBlockSize),
		m_fileName(file),
		m_file(nullptr), 
		m_queue(queue),
		m_isStopped(false),
		m_isEOF(false),
		m_needStop(false),
		m_errno(0)
	{
		// open file
#ifdef _WIN32
		const auto myErrno = fopen_s(&m_file, file.c_str(), "wb");
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
		// run a background thread of the file write by data from the queue
		m_backgroundWrite = make_unique<thread>(std::bind(&WriteStream::backgroundWrittingToFile, this));
	}

	virtual ~WriteStream()
	{
		if (!m_isEOF)
		{
			LOG(INFO) << "Start destucting with EOF "
				<< m_isEOF << ", Errno " << m_errno << ". Canceling write job";

			cancel();
		}
		LOG(INFO) <<  "Destuct: Is end of file " 
				<< m_isEOF << ", Errno " << m_errno ;

		m_queue.stopIncomes();
		m_backgroundWrite->join();
		LOG(INFO) << "Background stream of file write to disk is closed";
		fclose(m_file);
		LOG(INFO) << "The result file is closed";
		if (!m_isEOF || m_errno)
		{
			LOG(WARNING) << "The result file is removed. EOF " << m_isEOF << ". Errno " << m_errno;
			remove(m_fileName.c_str());
		}
	}

	void waitClose() override
	{
		if (m_errno)
		{
			throwOnFileError("Can't close file. It was errno: ", m_errno);
		}
		if (m_needStop)
		{
			throwOnFileError("Can't close file. Thread is canceled with errno: ", m_errno);
		}
		unique_lock<decltype(m_jobEndCVMutex)> lock(m_jobEndCVMutex);
		if (!m_jobEndCV.wait_for(lock, chrono::seconds(15), std::bind(&WriteStream::isStopped, this)))
		{
			LOG(WARNING) << "";
		}
	}

	void cancel() override
	{
		m_needStop = true;
		m_errno = EINTR;
		m_queue.pushError(EINTR, "Write to file is canceled by user thread");
	}

protected:
	bool isStopped() 
	{
		return m_isStopped;
	}

	bool isNeedToStopWrite()
	{
		return  m_queue.isInputStopped() || m_needStop || m_errno;
	}

	void flush(size_t& bytesToFlush)
	{
		if (fflush(m_file) != 0)
		{
			m_errno = errno;
			if (m_errno)
			{
				LOG(ERROR) << "File flush error " << m_errno ;
				m_isEOF = true;
			}
		}
		bytesToFlush = 0;
	}

	//Write a content from buffer to the file
	void backgroundWrittingToFile()
	{
		unique_lock<decltype(m_jobEndCVMutex)> lock(m_jobEndCVMutex);//It will unlocked on the end of job
		size_t totalWritten = 0;//bytes
		// It indicates how much bytes were written to the file without flush operation
		size_t bytesToFlush = 0; 
		try 
		{
			while (!isNeedToStopWrite())
			{
				auto ptr = std::move(m_queue.pop());
				if (!ptr) 
				{
					LOG(INFO) << "It's come a null from queue." << " Errno=" << m_errno <<
						", isEOF="<< m_isEOF << ". Continue";
					continue;
				}
				const size_t bufferSize = ptr->size();
				const size_t writeCount = fwrite(&(*ptr)[0], 1/*sizeof(char_type)*/, bufferSize, m_file);
				if (writeCount != bufferSize)
				{
					m_errno = errno;
					if (m_errno == 0)
					{
						continue;
					}
					//TODO: Returns buffer back on errno EINTR
					//Stop work
					m_queue.pushError(m_errno, "Error on file write.");
					break;
				}

				totalWritten += bufferSize;
				bytesToFlush += bufferSize;
				bool needFlush = (bytesToFlush >= m_ioBlockSize) ? true : false;
				if (needFlush)
				{
					flush(bytesToFlush);
				}
			}
			// Flush the rest of data to disk after end of the file 
			if (m_isEOF && !m_errno)
			{
				flush(bytesToFlush);
			}
		}
		catch (const std::exception& ex)
		{
			stringstream ss;
			ss << "Exception on background write to file: " << ex.what();
			LOG(ERROR) << ss.str();
			m_errno = EINTR;
			m_queue.pushError(EINTR, ss.str());
		}
		if (!m_errno)
		{
			flush(bytesToFlush);
		}
		m_isEOF = true;
		m_isStopped = true;
		LOG(INFO) << "Total bytes written: " << totalWritten << ". Is EOF=" << m_isEOF 
			<< " . Errno="<<m_errno;
	}


	unique_ptr<std::thread> m_backgroundWrite;

	const size_t m_ioBlockSize; // Optimal size of the block to flash on disk

	//Event of end background write
	mutex m_jobEndCVMutex;
	condition_variable m_jobEndCV;
	atomic<bool> m_isStopped;

	const std::string m_fileName;
	FILE *m_file;
	IStreamQueue &m_queue;
	// State of backgroundly processing file stream
	atomic<bool> m_isEOF;
	atomic<bool> m_needStop;
	atomic<int> m_errno;
};
};//end of namespace