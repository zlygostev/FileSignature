#pragma once
#include <string>
#include <vector>
#include <list>
#include <memory>

using namespace std;

namespace transformation_stream
{

using char_type = uint8_t;
using BufferT = vector<char_type>;
using BufferPTR = unique_ptr<BufferT>;
using ListOfBuffers = list<BufferPTR>;

void throwOnFileError(const std::string& description, int myErrno);

};