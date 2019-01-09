#pragma once

#include "CommonStreamBuffer.h"

namespace transformation_stream
{
	struct ITransformationStrategy
	{
		virtual ~ITransformationStrategy() = default;
		virtual void transform(BufferPTR data) = 0;
		virtual void dump() = 0;
	};
};//end of the namespace transformation_stream
