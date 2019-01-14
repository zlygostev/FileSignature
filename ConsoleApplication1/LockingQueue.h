#pragma once

#include "CommonStreamBuffer.h"

using namespace std;
#include <list>
#include <mutex>
#include <atomic>
#include <utility>
#include "IQueue.h"

namespace transformation_stream
{
// It's an implementation of IStreamQueue with
// - queue size bounds (by size in bytes)
// - active waiting on push and pop operations if the queue is full

class LockingQueue : public IStreamQueue
{
public:
	LockingQueue(size_t maxBufferSize, const std::string& queueName);

	virtual ~LockingQueue();

	void push(BlockPTR bufferPtr, bool isEndOfStream) override;

	void pushError(int inErrno, const std::string& msgDetails) override;

	void stopInputStream() override;

	bool isInputStopped() override;

	BlockPTR pop() override;

private:
	bool needReadThreadWakeup();

	bool isFreeSpaceEnoughForWrite(size_t dataSize);

	bool needWriteThreadWakeup(size_t dataSize);

	const std::string m_queueName; // Just for logs
	atomic<bool> m_isEOF;
	atomic<int> m_errno; // Not 0 if an error is occured
	std::string m_errnoMsg;

	const size_t m_maxBufferSize;//in bytes.
	std::atomic<size_t> m_QueueBytesSize;//in bytes. Atomic because in some cases uses without mutex. It's a bit faster

	//Events of read from buffer
	//mutex m_ReadEventsCVMutex;
	condition_variable m_ReadEventsCV;

	// buffer with prepared chunks for user
	ListOfBlocks m_buffers;
	mutex m_bufferMutex;

	//Events of background write to buffer from file
	//mutex m_WriteEventsCVMutex;
	condition_variable m_WriteEventsCV;

};

}// end of namespace