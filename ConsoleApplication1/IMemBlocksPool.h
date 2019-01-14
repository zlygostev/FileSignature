#pragma once
#include "CommonStreamBuffer.h"

namespace transformation_stream
{
struct IMemBlocksPool
{

	virtual BlockPTR get(size_t size) = 0;

	virtual void push(BlockPTR block) = 0;
};
}