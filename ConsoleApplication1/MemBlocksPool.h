#pragma once
#include <queue>
#include <mutex>
#include<memory>
#include "easylogging++.h"
#include "CommonStreamBuffer.h"
#include "IMemBlocksPool.h"

namespace transformation_stream
{
struct MemBlocksPool: IMemBlocksPool
{
	MemBlocksPool(size_t maxItemsCount):m_maxItemsCount(maxItemsCount)
	{
	}

	BlockPTR get(size_t size) override
	{
		unique_lock<decltype(m_mutex)> lock(m_mutex);
		if (!m_blocks.empty())
		{
			BlockPTR ptr = std::move(m_blocks.front());
			m_blocks.pop();
			lock.unlock();
			if (ptr->size() != size)
			{
				LOG(DEBUG) << "Resizing from " << ptr->size() << " to " << size;
				// it's a really rare case in our code 
				ptr->resize(size);
			}
			return ptr;
		}
		lock.unlock();
		LOG(DEBUG) << "No data in pool. MaxSize " << m_maxItemsCount;
		return make_unique<BlockT>(size);


	}
	void push(BlockPTR block) override
	{
		if (!block)
			return;

		unique_lock<decltype(m_mutex)> lock(m_mutex);
		if (m_blocks.size() < m_maxItemsCount)
		{
			m_blocks.push(std::move(block));
		}
		else
		{
			LOG(DEBUG) << "Remove buffer block";
		}
	}

protected:
	const size_t m_maxItemsCount;
	std::queue<BlockPTR> m_blocks;
	std::mutex m_mutex;
};
}