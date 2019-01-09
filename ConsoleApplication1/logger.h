#pragma once
#include "easylogging++.h"
//#include <boost/move/utility_core.hpp>
//#include <boost/log/sources/logger.hpp>
//#include <boost/log/trivial.hpp>
//#include <boost/log/sources/record_ostream.hpp>
//#include <boost/log/sources/global_logger_storage.hpp>
//#include <boost/log/utility/setup/file.hpp>
//#include <boost/log/utility/setup/common_attributes.hpp>
//#include <boost/log/sources/severity_logger.hpp>
//#include "CommonStreamBuffer.h"
//
//namespace logging = boost::log;
//namespace src = boost::log::sources;
//namespace keywords = boost::log::keywords;
//namespace sinks = boost::log::sinks;
//
////BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(sig_logger, src::logger_mt)
//
//namespace transformation_stream
//{
//namespace log
//{
//void init()
//{
//	logging::add_file_log
//	(
//		keywords::file_name = "signature%N.log",
//		keywords::rotation_size = 1 * options::units::MB,
//		keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0)//,
//		//keywords::format = "[%TimeStamp%]: %Message%"
//	);
//			
//	logging::core::get()->set_filter
//	(
//		logging::trivial::severity >= logging::trivial::info
//	);
//}

//#define trace BOOST_LOG_SEV(sig_logger::get(), trace) 
//#define debug BOOST_LOG_SEV(sig_logger::get(), debug) 
//#define info BOOST_LOG_SEV(sig_logger::get(), boost::trivial::severity_level::info) 
//#define warning BOOST_LOG_SEV(sig_logger::get(), warning) 
//#define error BOOST_LOG_SEV(sig_logger::get(), error) 
//#define fatal BOOST_LOG_SEV(sig_logger::get(), fatal) 
//}
//
//}
//#define GLOG_NO_ABBREVIATED_SEVERITIES
//#include <windows.h>

