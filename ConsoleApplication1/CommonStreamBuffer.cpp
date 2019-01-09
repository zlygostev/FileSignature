#pragma once
#include "CommonStreamBuffer.h"

#include <sstream>
#include <stdexcept>
#include "easylogging++.h"

namespace transformation_stream
{
	void throwOnFileError(const std::string& description, int myErrno)
	{
		if (myErrno)
		{
#ifdef _WIN32
			const size_t MAX_ERRNO_MESSAGE_SIZE = 94;
			char msgBuffer[MAX_ERRNO_MESSAGE_SIZE];
			strerror_s(msgBuffer, MAX_ERRNO_MESSAGE_SIZE, myErrno);
			std::stringstream ss;
			ss << description << ". Errno (" << myErrno << "): " << msgBuffer;
			auto errMsg = ss.str();
			LOG(ERROR) << errMsg;
			throw std::runtime_error(errMsg);
#else
			stringstream ss;
			ss << description << file << ". Errno (" << myErrno << "): " << strerror(myErrno);
			throw std::runtime_error(ss.str());
#endif
		}
	}
};