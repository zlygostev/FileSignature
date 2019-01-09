#pragma once
#include "IReadStream.h"
#include "IWriteStream.h"
#include "ITransformationStrategy.h"

#include <boost/uuid/name_generator_md5.hpp>
#include <iostream>
namespace transformation_stream
{

struct TransformationEngine
{
	TransformationEngine(IReadStream& in, IWriteStream& out, ITransformationStrategy& strategy) :
		m_in(in),
		m_out(out),
		m_transformationStrategy(strategy)
	{
	}

	void transform()
	{
		//TIMED_FUNC(TEtimerObj1);
		size_t totalSize = 0, readSize = 0;
		try
		{
			while (!m_in.isEOF())
			{
				//TIMED_SCOPE(TEtimerBlkObj2, "TransformLoop");
				auto bufferPtr = m_in.get();
				if (bufferPtr)
				{
					readSize = bufferPtr->size();
					m_transformationStrategy.transform(std::move(bufferPtr));
					totalSize += readSize;
				}
				else
				{
					LOG(INFO) <<  "No buffer come" ;
				}
			}
			m_transformationStrategy.dump();
			m_out.close();
			LOG(INFO) << "The file is read till the end. Size " << totalSize;
		}
		catch (const std::exception& ex)
		{
			LOG(ERROR) << "Stop transformation by exception on byte " << totalSize << ". Error: " << ex.what();
			m_in.stop();
			m_out.cancel();
			throw ex;
		}
	}

	IReadStream& m_in;
	IWriteStream& m_out;
	ITransformationStrategy& m_transformationStrategy;

};
}//end of namespace  transformation_stream