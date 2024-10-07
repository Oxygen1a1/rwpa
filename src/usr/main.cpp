#include <iostream>
#include <format>
#include <string>
#include <ranges>
#include <algorithm>
#include <Windows.h>
#include <algorithm>

#include <kioctl.hpp>

constexpr auto CTL_KRWPA = CTL_CODE(0x8000, 0x0823, METHOD_BUFFERED, FILE_ANY_ACCESS);

// read write phisics address struct
struct RWPA
{
	ULONG64 va;
	ULONG64 pa;
	ULONG64 size;
	ULONG64 is_write;
};
static_assert(sizeof(RWPA) == 32, "RWPA size is not 32 bytes");

int main()
{
	std::cout << std::format("{:*^40}", "") << std::endl;
	std::ranges::for_each(std::views::iota(0, 3), [](int)
						  { std::cout << std::format("*{: ^38}*", "") << std::endl; });
	std::cout << std::format("*{: ^38}*", "read write physics(v0.1)", "") << std::endl;
	std::ranges::for_each(std::views::iota(0, 3), [](int)
						  { std::cout << std::format("*{: ^38}*", "") << std::endl; });
	std::cout << std::format("{:*^40}", "") << std::endl;

	kstd::Driver drv(L"drv.sys");
	drv.start();

	std::cout << "[info]:press the Q key to exit" << std::endl;
	while (1)
	{
		std::string cmd;
		std::cin >> cmd;

		if (cmd == "Q")
		{
			break;
		}
		else if (cmd.find("db") != std::string::npos)  // command like [db] [addr] [len]
		{
			std::string addr, len;
			std::cin >> addr;
			std::cin >> len;

			auto pa   = std::stoull(addr, nullptr, 16);
			auto size = std::stoull(len, nullptr, 16);
			// allocate memory to store the read data
			auto        buf  = std::make_unique<unsigned char[]>(size + 10 /*addational bytes*/);
			auto        va   = reinterpret_cast<ULONG64>(buf.get());
			struct RWPA rwpa = { va, pa, size, 0 };

			// ioctl to read physical memory
			if (!drv.ioctl(CTL_KRWPA, &rwpa, sizeof rwpa, nullptr, 0))
			{
				std::cout << std::format("[error]:failed to read pa:{:x}", pa) << std::endl;
			}
			else
			{
				// if success,print the data
				std::for_each(buf.get(), buf.get() + size, [](unsigned char byte)
							  {
                                static int count = 0;
                                if(count++ % 16 == 0)
                                {
                                    std::cout << std::endl;
                                }
								std::cout << std::format("{:02x} ", byte); });
				std::cout << std::endl;
			}
		}
		else if (cmd.find("eb") != std::string::npos)  // command like [eb] [addr] [byte sequence]
		{
			std::string input;
			std::getline(std::cin, input);
			std::vector<unsigned char> binary_sequence;
			std::string                addr;
			std::istringstream         iss(input);
			std::string                hex_str;
			// first skip the begin with cmd 'db'
			iss >> addr;
			while (iss >> hex_str)
			{
				if (hex_str.substr(0, 2) == "0x")
				{
					hex_str = hex_str.substr(2);
				}
				unsigned int       byte;
				std::istringstream hex_iss(hex_str);
				if (hex_iss >> std::hex >> byte)
				{
					binary_sequence.push_back(static_cast<unsigned char>(byte));
				}
				else
				{
					std::cerr << "[error]:Invalid hex value: " << hex_str << "\n";
					break;
				}
			}
			auto        pa   = std::stoull(addr, nullptr, 16);
			auto        va   = reinterpret_cast<ULONG64>(binary_sequence.data());
			auto        size = binary_sequence.size();
			struct RWPA rwpa = { va, pa, size, 1 };

			// ioctl to write physical memory
			if (!drv.ioctl(CTL_KRWPA, &rwpa, sizeof rwpa, nullptr, 0))
			{
				std::cout << std::format("[error]:failed to write pa:{:x}", addr) << std::endl;
			}
		}
		else
		{
			std::cout << "[error]:unknow command,please input again" << std::endl;
		}
	}

	drv.stop();
	return 0;
}