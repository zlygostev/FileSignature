
#include "Options.h"
#include "ReadStreamBuffer.h"
#include "WriteSteamBuffer.h"
#include "MD5SignatureCalculationStrategy.h"
#include "TransformationEngine.h"
#include "LockingQueue.h"
#include "MemBlocksPool.h"
#include <iostream>
//#include <direct.h>
//#include "logger.h"
//#include <boost/log/trivial.hpp>
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP


using namespace transformation_stream;

int main(int argc, const char* argv[])
{
	el::Configurations conf("logger.config");
	el::Loggers::reconfigureAllLoggers(conf);
	try
	{

		LOG(INFO) << "Starting";

		// Get startup settings from commandline options
		options::UtilityOptions opts;
		opts.Parse(argc, argv);
		auto action = opts.GetAction();
		if (action != options::Action::GetSignature)
		{
			opts.ShowHelp();
		}
		auto settings = opts.GetSignatureSettings();
		settings.check(); //throw on inacceptable settings

		
		// It was an idea to read the file by a few big blocks parallel and save their signatures. 
		// But the idea has a few bad cases:
		// - seek penaltes
		// - sample block size could has a lower value then size of md5.
		// - high disk load on run
		// So as more clear and easiest solution has been made a next solution:
		// Threads conveyer
		// Queues for conveyor organization
		// Pool takes a good affect on big files and large ioPortionSize
		MemBlocksPool memPool(settings.maxBufferSize/settings.ioPortionSize + 1);
		LockingQueue inputQueue(settings.maxBufferSize, "InQueue");
		LockingQueue outputQueue(settings.maxBufferSize, "OutQueue");
		// One thread is sequentually reading input file to the inputQueue in an individual thread
		ReadStream inputStream(settings.source, inputQueue, memPool, settings.ioPortionSize);
		// Another thread realizes output stream. It writes data from outputQueue to result file backgroundly 
		WriteStream outputStream(settings.result, outputQueue, settings.ioPortionSize);
		// There is a main thread that get chunks of the input file from inputQueue, 
		// calculate their hashes and write them to the output queue.
		MD5SignatureCalculationStrategy transformationStrategy(outputQueue, memPool, settings.sampleSize);
		TransformationEngine engine(inputQueue, outputQueue, transformationStrategy);
		LOG(INFO) << "Start transformation";
		engine.transform();
		LOG(INFO) << "Finish transformation";
		outputStream.waitClose();
		LOG(INFO) << "Main destructors run";

	}
	catch (std::exception& ex)
	{ 
		LOG(ERROR) << "Stop transformation. " << ex.what();
		std::cerr << "Error: " << ex.what() << std::endl;
		return -1;
	}
	LOG(INFO) << "Finishing";
	return 0;
}