
#include "Options.h"
#include "ReadStreamBuffer.h"
#include "WriteSteamBuffer.h"
#include "MD5SignatureCalculationStrategy.h"
#include "TransformationEngine.h"
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

		
		//It was an idea to read the file by a few big blocks parallel and save their signatures. 
		// But the idea has a few bad cases:
		// - (for example) sample block size could has a lower value then size of md5.
		// - high disk loaCd on run
		// So as more clear and easiest solution has been made next solution:
	
		// One thread is sequentually reading input file to a buffer in one thread
		ReadStream inputStream(settings.source, settings.maxBufferSize, settings.ioPortionSize);
		// Another thread realizes output stream. It writes data from buffer to result file backgroundly 
		WriteStream outputStream(settings.result, settings.maxBufferSize, settings.ioPortionSize);
		// There is a main thread that get chunks of the input file from buffer, 
		// calculate their hashes and write them to the output steam.
		MD5SignatureCalculationStrategy transformationStrategy(outputStream, settings.sampleSize);
		TransformationEngine engine(inputStream, outputStream, transformationStrategy);
		LOG(INFO) << "Start transformation";
		engine.transform();
		LOG(INFO) << "Finish transformation";
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