#include "InputStreamBuffer.h"



InputStreamBuffer::InputStreamBuffer(const std::string& file, size_t buffer_size):
	m_maxBufferSize(buffer_size),
	m_buffer(buffer_size),
	m_isEOF(false),
	m_isStop(false)
{
}