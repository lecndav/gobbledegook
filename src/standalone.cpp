#include <signal.h>
#include <iostream>
#include <thread>
#include <sstream>
#include <vector>
#include <regex>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <assert.h>

#include "../include/Gobbledegook.h"

//
// Constants
//

// Maximum time to wait for any single async process to timeout during initialization
static const int kMaxAsyncInitTimeoutMS = 30 * 1000;

static const std::string networkInterfaceName = "wlan0";

//
// Server data values
//

// String array of example text strings

std::string wifiSSID = "";
std::string wifiPassword = "";

// struct that holds ssid and rssi
struct SSID
{
	std::string name;
	uint8_t rssi;
};
std::vector<SSID> SSIDs;
int requestedSSIDIndex = 0;

//
// Logging
//

enum LogLevel
{
	Debug,
	Verbose,
	Normal,
	ErrorsOnly
};

// Our log level - defaulted to 'Normal' but can be modified via command-line options
LogLevel logLevel = Normal;

// Our full set of logging methods (we just log to stdout)
//
// NOTE: Some methods will only log if the appropriate `logLevel` is set
void LogDebug(const char *pText)
{
	if (logLevel <= Debug)
	{
		std::cout << "  DEBUG: " << pText << std::endl;
	}
}
void LogInfo(const char *pText)
{
	if (logLevel <= Verbose)
	{
		std::cout << "   INFO: " << pText << std::endl;
	}
}
void LogStatus(const char *pText)
{
	if (logLevel <= Normal)
	{
		std::cout << " STATUS: " << pText << std::endl;
	}
}
void LogWarn(const char *pText) { std::cout << "WARNING: " << pText << std::endl; }
void LogError(const char *pText) { std::cout << "!!ERROR: " << pText << std::endl; }
void LogFatal(const char *pText) { std::cout << "**FATAL: " << pText << std::endl; }
void LogAlways(const char *pText) { std::cout << "..Log..: " << pText << std::endl; }
void LogTrace(const char *pText) { std::cout << "-Trace-: " << pText << std::endl; }

//
// Network interface functions
//

uint8_t isNetworkInterfaceUp()
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	ifreq ifr;
	strncpy(ifr.ifr_name, networkInterfaceName.c_str(), IFNAMSIZ - 1);
	ioctl(sock, SIOCGIFFLAGS, &ifr);
	uint8_t ret;

	if (ifr.ifr_flags & IFF_UP)
	{
		std::cout << "wlan0 is up" << std::endl;
		ret = 1;
	}
	else
	{
		std::cout << "wlan0 is down" << std::endl;
		ret = 0;
	}

	close(sock); // close the socket
	return ret;
}

void setNetworkInterfaceUp()
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	ifreq ifr;
	strncpy(ifr.ifr_name, networkInterfaceName.c_str(), IFNAMSIZ - 1);
	ioctl(sock, SIOCGIFFLAGS, &ifr);
	ifr.ifr_flags |= IFF_UP;
	ioctl(sock, SIOCSIFFLAGS, &ifr);
	close(sock); // close the socket
}

void bringUpNetworkInterface()
{
	if (!isNetworkInterfaceUp())
	{
		LogStatus("Bringing up network interface");
		setNetworkInterfaceUp();
	}
}

uint8_t getRSSIFromString(std::string str)
{
	std::regex re("[-+]?([0-9]*)\\.?[0-9]+");
	std::smatch match;
	std::regex_search(str, match, re);
	return std::stoi(match[1].str());
}

std::string getSSIDNameFromString(std::string str)
{
	std::regex re("SSID: (.*)");
	std::smatch match;
	std::regex_search(str, match, re);
	return match[1].str();
}

// function to scan for available wifi networks and signal with command "iw dev wlan0 scan | grep -e SSID -e signal"
void scanWifiNetworks()
{
	bringUpNetworkInterface();
	std::string cmd = "iw dev wlan0 scan | grep -e SSID -e signal";
	std::string data;
	FILE *stream;
	const int max_buffer = 256;
	char buffer[max_buffer];
	cmd.append(" 2>&1");

	stream = popen(cmd.c_str(), "r");
	if (stream)
	{
		while (!feof(stream))
		{
			if (fgets(buffer, max_buffer, stream) != NULL)
			{
				data.append(buffer);
			}
		}
		pclose(stream);
	}

	std::vector<std::string> wifiNetworks;
	std::istringstream f(data);
	std::string line;
	uint8_t i = 0;
	std::string name;
	uint8_t rssi = 0;
	while (std::getline(f, line))
	{
		if (i % 2 == 0)
		{
			rssi = getRSSIFromString(line);
		}
		else
		{
			name = getSSIDNameFromString(line);
			if (name.empty())
			{
				i++;
				continue;
			}
			const SSID ssid = {name, rssi};
			SSIDs.push_back(ssid);
		}
		i++;
	}
}

//
// Signal handling
//

// We setup a couple Unix signals to perform graceful shutdown in the case of SIGTERM or get an SIGING (CTRL-C)
void signalHandler(int signum)
{
	switch (signum)
	{
	case SIGINT:
		LogStatus("SIGINT recieved, shutting down");
		ggkTriggerShutdown();
		break;
	case SIGTERM:
		LogStatus("SIGTERM recieved, shutting down");
		ggkTriggerShutdown();
		break;
	}
}

//
// Server data management
//

// Called by the server when it wants to retrieve a named value
//
// This method conforms to `GGKServerDataGetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
const void *dataGetter(const char *pName)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data getter");
		return nullptr;
	}

	std::string strName = pName;

	if (strName == "ssid/res")
	{
		LogInfo((std::string("requested index '") + std::to_string(requestedSSIDIndex) + "'").c_str());
		if (requestedSSIDIndex >= SSIDs.size())
		{
			return nullptr;
		}

		const auto nameLength = SSIDs[requestedSSIDIndex].name.length();
		assert(nameLength < 32);

		static uint8_t data[34];
		memset(data, 0, sizeof(data));

		data[0] = SSIDs[requestedSSIDIndex].rssi;
		memcpy((void *)(data+1), SSIDs[requestedSSIDIndex].name.c_str(), nameLength);

		data[nameLength + 1] = (requestedSSIDIndex + 1 >= SSIDs.size()? 0xff : requestedSSIDIndex + 1);
		return data;
	}

	if (strName == "gateway/status")
	{
		static const std::string wifiStatus = "Connected";
		return &wifiStatus;
	}

	LogWarn((std::string("Unknown name for server data getter request: '") + pName + "'").c_str());
	return nullptr;
}

// Called by the server when it wants to update a named value
//
// This method conforms to `GGKServerDataSetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
int dataSetter(const char *pName, const void *pData)
{
	if (nullptr == pName)
	{
		LogError("NULL name sent to server data setter");
		return 0;
	}
	if (nullptr == pData)
	{
		LogError("NULL pData sent to server data setter");
		return 0;
	}

	std::string strName = pName;

	if (strName == "ssid/start")
	{
		requestedSSIDIndex = 0;
		scanWifiNetworks();

		LogInfo((std::string("Size of vecotr is: '") + std::to_string(SSIDs.size()) + "'").c_str());
		ggkNofifyUpdatedCharacteristic("/com/nxt/ssid/res");
		return 1;
	}

	if (strName == "ssid/id")
	{
		requestedSSIDIndex = static_cast<const uint8_t *>(pData)[0];
		ggkNofifyUpdatedCharacteristic("/com/nxt/ssid/res");
		return 1;
	}

	else if (strName == "ssid/password")
	{
		wifiPassword = static_cast<const char *>(pData);
		LogDebug((std::string("Server data: text string set to '") + wifiPassword + "'").c_str());
		return 1;
	}
	else if (strName == "ssid/name")
	{
		wifiSSID = static_cast<const char *>(pData);
		LogDebug((std::string("Server data: text string set to '") + wifiSSID + "'").c_str());
		return 1;
	}

	LogWarn((std::string("Unknown name for server data setter request: '") + pName + "'").c_str());

	return 0;
}

int main(int argc, char **ppArgv)
{
	// A basic command-line parser
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = ppArgv[i];
		if (arg == "-q")
		{
			logLevel = ErrorsOnly;
		}
		else if (arg == "-v")
		{
			logLevel = Verbose;
		}
		else if (arg == "-d")
		{
			logLevel = Debug;
		}
		else
		{
			LogFatal((std::string("Unknown parameter: '") + arg + "'").c_str());
			LogFatal("");
			LogFatal("Usage: standalone [-q | -v | -d]");
			return -1;
		}
	}

	// Setup our signal handlers
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	// Register our loggers
	ggkLogRegisterDebug(LogDebug);
	ggkLogRegisterInfo(LogInfo);
	ggkLogRegisterStatus(LogStatus);
	ggkLogRegisterWarn(LogWarn);
	ggkLogRegisterError(LogError);
	ggkLogRegisterFatal(LogFatal);
	ggkLogRegisterAlways(LogAlways);
	ggkLogRegisterTrace(LogTrace);

	// set service uuid here because gobbledegook doesn't support it yet
	std::string cmd = "hcitool -i hci0 cmd 0x08 0x0008 12 11 07 5F C8 66 32 CE 66 45 81 79 46 C8 B0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00";
	system(cmd.c_str());

	if (!ggkStart("nxt", "NXT Provisioner", "NXT", dataGetter, dataSetter, kMaxAsyncInitTimeoutMS))
	{
		return -1;
	}

	// Wait for the server to come to a complete stop (CTRL-C from the command line)
	if (!ggkWait())
	{
		return -1;
	}

	// Return the final server health status as a success (0) or error (-1)
	return ggkGetServerHealth() == EOk ? 0 : 1;
}
