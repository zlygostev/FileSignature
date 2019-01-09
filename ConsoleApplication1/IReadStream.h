#pragma once

#include "CommonStreamBuffer.h"

namespace transformation_stream
{
	struct IQueue
	{
		virtual void push(BufferPTR) = 0;
		virtual BufferPTR pop() = 0;
		// returns size in bytes of all blocks of data
		virtual size_t dataSize() = 0;
		// returns items count in buffer
		virtual size_t itemsCount() = 0;

	};

	struct IReadStream
	{
		virtual ~IReadStream() = default;

		// Returns chunk of file or emptyPtr if file is end
		// Could throw runtime_error on error cases
		virtual BufferPTR get() = 0;

		// Check if end of file is occured
		virtual bool isEOF() = 0;

		// stop background read of file
		virtual void stop() = 0;
	};

};//end of the namespace transformation_stream
#pragma once
