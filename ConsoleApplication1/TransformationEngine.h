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
	TransformationEngine(IStreamQueue& in, IStreamQueue& out, ITransformationStrategy& strategy) :
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
			while (!m_in.isInputStopped())
			{
				//TIMED_SCOPE(TEtimerBlkObj2, "TransformLoop");
				auto bufferPtr = m_in.pop();
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
			m_out.stopIncomes();
			LOG(INFO) << "The file is read till the end. Size " << totalSize;
		}
		catch (const std::exception& ex)
		{
			LOG(ERROR) << "Stop transformation by exception on byte " << totalSize << ". Error: " << ex.what();
			m_in.stopIncomes();
			m_out.pushError(EINTR, ex.what());
			throw ex;
		}
	}

	IStreamQueue& m_in;
	IStreamQueue& m_out;
	ITransformationStrategy& m_transformationStrategy;

};
}//end of namespace  transformation_stream