#pragma once

#include <boost/uuid/name_generator_md5.hpp>

#include "IWriteStream.h"
#include "IReadStream.h"
#include "IQueue.h"
#include "MemBlocksPool.h"
#include "ITransformationStrategy.h"

namespace transformation_stream
{
//Class implements logic of MD5 signature file build
struct MD5SignatureCalculationStrategy: ITransformationStrategy
{
	MD5SignatureCalculationStrategy(IStreamQueue& out, MemBlocksPool& memPool, size_t portion_size);

	void transform(BlockPTR data) override;

	void dump() override;

private:
	IStreamQueue& m_out;
	MemBlocksPool& m_memPool;
	const size_t m_portionSize;
	size_t m_transformedCount;
	std::unique_ptr<boost::uuids::detail::md5> m_md5;
	size_t m_blockWritten;

};

};//end of the namespace transformation_stream
