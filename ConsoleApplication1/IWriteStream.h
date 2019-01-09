#pragma once

#include "CommonStreamBuffer.h"

namespace transformation_stream
{
	//Put files by blocks to file
	struct IWriteStream
	{
		virtual ~IWriteStream() = default;

		// Put data to the output file asynchronously
		virtual void putAsync(BufferPTR chunk) = 0;

		// Indicate the end of the file data 
		virtual void close() = 0;

		// Cancel output stream. Remove output file 
		virtual void cancel() = 0;
	};
};//end of the namespace transformation_stream
#pragma once
