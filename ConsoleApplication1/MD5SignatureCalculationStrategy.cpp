#include "MD5SignatureCalculationStrategy.h"
#include "MemBlocksPool.h"
#include <boost/algorithm/hex.hpp>
#include "easylogging++.h"

namespace transformation_stream
{

MD5SignatureCalculationStrategy::MD5SignatureCalculationStrategy(IStreamQueue& out, MemBlocksPool& memPool, size_t portion_size) :
	m_out(out),
	m_memPool(memPool),
	m_portionSize(portion_size),
	m_transformedCount(0),
	m_blockWritten(0)
{
	m_md5 = make_unique<boost::uuids::detail::md5>();
}

void MD5SignatureCalculationStrategy::transform(BlockPTR data)
{
	
	if (!data)
		return;

	LOG(DEBUG) << "Start transform chunk of data size " << data->size();
 	size_t dataShift = 0;
	for (auto dataSize = data->size(); dataSize > 0;)
	{
		if (dataSize < m_portionSize - m_transformedCount)
		{
			m_md5->process_bytes(&((*data)[0]) + dataShift, dataSize);
			dataShift += dataSize;
			m_transformedCount += dataSize;
			dataSize = 0;
			break;
		}

		// Buffer is larger or equal then block for hash
		// Fullfill block for hash calculation
		LOG(DEBUG) << "Fullfill block for hash calculation size " << m_portionSize - m_transformedCount << 
			", dataShift " << dataShift;
		m_md5->process_bytes(&((*data)[0]) + dataShift, m_portionSize - m_transformedCount);
		LOG(DEBUG) << "Hash calculation finished.";

		// Shift buffer
		dataSize -= m_portionSize - m_transformedCount;
		dataShift += m_portionSize - m_transformedCount;
		m_transformedCount += m_portionSize - m_transformedCount;

		// Put hash in file
		LOG(DEBUG) << "get hash bytes";
		dump();

		LOG(DEBUG) << "create new hash";
		// Create new MD5 for calculation
		m_md5 = make_unique<boost::uuids::detail::md5>();
		m_transformedCount = 0;//reset calculation state
		LOG(DEBUG) << "Hash calculation is finished";
	}
	m_memPool.push(std::move(data));
}

void MD5SignatureCalculationStrategy::dump()
{
	if (m_transformedCount == 0)
		return;

	if (m_transformedCount != m_portionSize)
	{
		LOG(INFO) << "Dump MD5 portion of size " << m_transformedCount << " less then " << m_portionSize;
	}
	boost::uuids::detail::md5::digest_type digest;
	m_md5->get_digest(digest);
	uint8_t* tmpBufferPtr = reinterpret_cast<uint8_t*>(&(digest[0]));
	const size_t MD5BytesSize = 16; //sizeof(digest)
	BlockPTR buffer = make_unique<BlockT>(tmpBufferPtr, tmpBufferPtr + MD5BytesSize);
	//std::string md5Text;
	//boost::algorithm::hex(buffer->begin(), buffer->end(), back_inserter(md5Text));
	//LOG(TRACE) << "New md5: " << md5Text;
	m_out.push(std::move(buffer));
	m_blockWritten++;
}

};//end of the namespace transformation_stream