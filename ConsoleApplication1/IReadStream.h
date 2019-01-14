#pragma once

#include "CommonStreamBuffer.h"

namespace transformation_stream
{

	struct IReadStream
	{
		virtual ~IReadStream() = default;

		// Returns chunk of file or emptyPtr if file is end
		// Could throw runtime_error on error cases
		//virtual BlockPTR get() = 0;

		// Check if end of file is occured
		virtual bool isEOF() = 0;

		// stop background read of file
		virtual void stop() = 0;
	};

};//end of the namespace transformation_stream
#pragma once
