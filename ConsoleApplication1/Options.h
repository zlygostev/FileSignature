#pragma once
#include <boost/program_options.hpp>
#include <string>
#include <stdexcept>
#include <iostream>

namespace po = boost::program_options;

namespace transformation_stream
{
namespace options
{
	enum class Action
	{
		None = 0,
		GetHelp = 1,
		GetSignature = 2
	};

	namespace units
	{
		static constexpr size_t KB = 1024;//Byte
		static constexpr size_t MB = 1024 * KB;//Byte
	}

	struct SignatureSettings
	{
		std::string source = { "source.txt" };
		std::string result = { "signature.out" };
		size_t sampleSize = { 1 * units::MB };
		size_t ioPortionSize = { 1 * units::MB };
		size_t maxBufferSize = { 3 * units::MB };

		void check()
		{
			if (source.empty()) {
				throw std::invalid_argument("The source file should be set as not empty.");
			}
			if (result.empty()) {
				throw std::invalid_argument("The result file name should be set as not empty.");
			}
			if (sampleSize <= 0) {
				throw std::invalid_argument("The sample block size should have a positive value.");
			}
			if (ioPortionSize <= 0) {
				throw std::invalid_argument("io-buffer size should have a positive value.");
			}
			if (maxBufferSize < 2 * ioPortionSize)
			{
				std::string err = "Maximal buffer size should be larger at list twice of IO block operation. "
					"It is a performance optimization of the parallel work";
				throw std::invalid_argument(err);
			}

		}
	};


	struct UtilityOptions
	{

		UtilityOptions()
		{
			m_description.add_options()
				("help,h", "produce help message")
				("source,i", po::value<std::string>(&m_sigSettings.source),
					"a path to a file for a farther signature calculations. Default: source.txt")
					("signature,o", po::value<std::string>(&m_sigSettings.result),
						"a path to a result file with a sinature. Default: signature.out")
						("blocksize,s", po::value<size_t>(&m_sigSettings.sampleSize),
							"a size (in bytes) of the signature sample block. Default is 1MB")
							("ioblock,b", po::value<size_t>(&m_sigSettings.ioPortionSize),
								"a size (in bytes) of the block for communication with a file system. Default is 1 MB")
								("iobuffer,c", po::value<size_t>(&m_sigSettings.maxBufferSize),
									"a size (in bytes) of the buffer for background data caching. Default is 3 MB");
		}

		void Parse(int argc, const char* argv[])
		{
			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, m_description), vm);
			po::notify(vm);
			if (vm.count("help"))
			{
				m_action = Action::GetHelp;
				return;
			}
			if (!m_sigSettings.source.empty() && !m_sigSettings.result.empty())
			{
				m_action = Action::GetSignature;
				return;
			}
		}

		Action GetAction() { return m_action; }

		SignatureSettings GetSignatureSettings() { return m_sigSettings; }

		void ShowHelp()
		{
			std::cout << m_description << std::endl;
		}

	private:
		SignatureSettings m_sigSettings;
		Action m_action = { Action::None };
		po::options_description m_description = { "Allowed options" };

	};
} // end of namespace options
} // end of namespace transformation_stream