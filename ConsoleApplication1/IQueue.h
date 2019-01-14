#pragma once
#include "CommonStreamBuffer.h"


namespace transformation_stream
{
// This is a queue for store events and chunks of stream for farther processing
// That's why the queue has a few methods to store stream errors and event of the end of stream.
struct IStreamQueue
{
	virtual void push(BlockPTR, bool isEndOfStream = false) = 0;

	// This method should be a last one of push methods. It assumes stop of stream after
	virtual void pushError(int inErrno, const std::string& msg) = 0;

	// Set it to prohibit any push operations to queue and indicates an end of the input stream
	virtual void stopIncomes() = 0;

	virtual bool isInputStopped() = 0;

	virtual BlockPTR pop() = 0;
};

}