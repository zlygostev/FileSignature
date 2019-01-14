#pragma once
#include <string>
#include <vector>
#include <list>
#include <memory>

using namespace std;

namespace transformation_stream
{

using char_type = uint8_t;
using BlockT = vector<char_type>;
using BlockPTR = unique_ptr<BlockT>;
using ListOfBlocks = list<BlockPTR>;

void throwOnFileError(const std::string& description, int myErrno);

};