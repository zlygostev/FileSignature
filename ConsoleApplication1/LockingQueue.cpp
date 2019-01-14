#include "LockingQueue.h"
#include "easylogging++.h"


namespace transformation_stream
{
LockingQueue::LockingQueue(size_t maxBufferSize, const std::string& queueName) : m_queueName(queueName)
																				, m_isEOF(false)
																				, m_errno(0)
																				, m_maxBufferSize(maxBufferSize)\
																				, m_QueueBytesSize(0)
{
	if (m_maxBufferSize == 0)
	{
		throw std::invalid_argument("max Buffer size should have positive value.");
	}

}

LockingQueue::~LockingQueue()
{
	stopInputStream();

}
void LockingQueue::push(BlockPTR bufferPtr, bool isEndOfStream)
{
	if (!bufferPtr)
	{
		LOG(WARNING) << m_queueName << "empty chunk is come";
		return;
	}

	const auto bufSize = bufferPtr->size();
	if (bufSize > m_maxBufferSize)
	{
		stringstream msg_stream;
		msg_stream << "Stream error: Attempt to write asynchroniusly chunk of data " <<
			bufSize << " larger then maximum buffer size (" << m_maxBufferSize << ")";

		LOG(ERROR) << m_queueName << ": " << msg_stream.str();
		throw invalid_argument(msg_stream.str());
	}

	bool isPushed = false;
	for (size_t attemptsCount = 0; !isPushed && !m_isEOF; ++attemptsCount)
	{
		LOG(DEBUG) << m_queueName << ":Pushing block (" << bufSize
			<< "B) in queue. Queue size: " << m_QueueBytesSize << "B. Attmpt: " << attemptsCount;

		//It impossibe to make a pushing in queue after any error
		if (m_errno)
		{
			std::string err = m_errnoMsg;
			LOG(INFO) << m_queueName << ": " << err <<
				"throw errno exception from queue on push. Errno: " << m_errno;

			throwOnFileError(err, m_errno);
		}

		//If stream is indecated as finished, no way to push s.t. else
		if (m_isEOF)
		{
			std::string msg = "the queue's stream is already closed";
			LOG(ERROR) << m_queueName << ": " << msg;
			throw invalid_argument(msg);
		}
		unique_lock<decltype(m_bufferMutex)> lock(m_bufferMutex);
		if (!isFreeSpaceEnoughForWrite(bufSize))
		{
			// Wait if buffer full
			if (!m_ReadEventsCV.wait_for(lock, chrono::milliseconds(15000),
				std::bind(&LockingQueue::needWriteThreadWakeup, this, bufSize)))
			{
				lock.unlock();// An optimization for exclude log output from a locked session
				LOG(WARNING) << m_queueName << ": Timeout. The queue is full. Still need wait a space for a pushing of " << bufSize << " (B). Buffer size "
					<< m_QueueBytesSize << ". Attempt" << attemptsCount << ". Is EOF=" << m_isEOF;
			}
			continue;//If thread is waited free space let's try again from start
		}

		//Add data in the queue
		m_buffers.push_back(std::move(bufferPtr));
		m_QueueBytesSize += bufSize;
		if (isEndOfStream)
		{
			m_isEOF = isEndOfStream;
		}
		lock.unlock();// An optimization for exclude log output from a locked session
		LOG(DEBUG) << m_queueName << ": A new block is add-ed. The size is " << bufSize
			<< " (B). The total queue size is " << m_QueueBytesSize << "B . Attempt "
			<< attemptsCount << ". Is EOF=" << m_isEOF;

		m_WriteEventsCV.notify_one();
		isPushed = true;//or break?
	}
}

void LockingQueue::pushError(int inErrno, const std::string& msgDetails)
{
	LOG(WARNING) << m_queueName << ": It's come error " << inErrno
		<< " with message: " << msgDetails;

	m_errno = inErrno;
	m_errnoMsg = msgDetails;
	stopInputStream();
}

void LockingQueue::stopInputStream()
{
	LOG(WARNING) << m_queueName << ": Request of stop input stream is come. Is EOF " << m_isEOF;
	if (!m_isEOF)
	{
		m_isEOF = true;
		m_WriteEventsCV.notify_one();
		m_ReadEventsCV.notify_one();
	}
}

bool LockingQueue::isInputStopped()
{
	return m_isEOF && (m_QueueBytesSize == 0);
}

BlockPTR LockingQueue::pop()
{
	do
	{
		LOG(DEBUG) << m_queueName << ": An attempt to get data from buffer";
		unique_lock<decltype(m_bufferMutex)> lock(m_bufferMutex);
		if (!m_buffers.empty())
		{
			BlockPTR ptr = std::move(m_buffers.front());
			const auto bufSize = ptr->size();
			m_QueueBytesSize -= bufSize;
			m_buffers.pop_front();
			lock.unlock();
			LOG(DEBUG) << m_queueName << ": Extracted chunk " << bufSize << " B by user. Buffer size " << m_QueueBytesSize;
			m_ReadEventsCV.notify_one();
			//TODO: is it really need std::move here?
			return std::move(ptr);
		}
		if (m_isEOF)
		{
			if (m_errno)
			{
				std::string err = m_errnoMsg;
				LOG(INFO) << m_queueName << ": " << err << "throw errno exception from queue. Errno: " << m_errno;
				throwOnFileError(err, m_errno);
			}
			//TODO: is it really need std::move here?
			return std::move(BlockPTR(nullptr));
		}
		//wait New Data
		LOG(DEBUG) << m_queueName << ": Wait a new data in queue inside pop(). EOF=" << m_isEOF;
		if (!m_WriteEventsCV.wait_for(lock, chrono::milliseconds(15000),
			std::bind(&LockingQueue::needReadThreadWakeup, this)))
		{
			LOG(WARNING) << m_queueName << ": Timeout on background write waiting. Buffers " << m_QueueBytesSize;
		}
		lock.unlock();
		LOG(DEBUG) << m_queueName << ": Stop waiting of new data in queue";
	} while (!(m_isEOF && m_QueueBytesSize == 0));
	//TODO: is it really need std::move here?
	return std::move(BlockPTR(nullptr));
}


bool LockingQueue::needReadThreadWakeup()
{
	return m_isEOF || m_QueueBytesSize > 0;
}

bool LockingQueue::isFreeSpaceEnoughForWrite(size_t dataSize)
{
	return static_cast<int64_t>(dataSize) <= static_cast<int64_t>(m_maxBufferSize - m_QueueBytesSize);
}

bool LockingQueue::needWriteThreadWakeup(size_t dataSize)
{
	return m_isEOF || isFreeSpaceEnoughForWrite(dataSize);
}

};