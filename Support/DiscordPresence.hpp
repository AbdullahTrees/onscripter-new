/**
 *  DiscordPresence.hpp
 *  ONScripter-RU
 *
 *  Minimal Discord Rich Presence client over local Discord RPC IPC.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

class DiscordPresence {
public:
	struct Activity {
		std::string details;
		std::string state;
		std::string largeImage;
		std::string largeText;
	};

	bool start(const std::string &applicationId);
	void update(const Activity &activity);
	void service();
	void shutdown();

	bool isEnabled() const {
		return enabled;
	}

private:
	bool connect();
	bool openConnection();
	void closeConnection();
	bool hasReadableData();
	bool writeFrame(uint32_t opcode, const std::string &payload);
	bool readFrame(uint32_t &opcode, std::string &payload, uint32_t timeoutMs);
	bool writeAll(const uint8_t *data, size_t size);
	bool readExact(uint8_t *data, size_t size, uint32_t timeoutMs);
	bool handleFrame(uint32_t opcode, const std::string &payload);
	void sendActivity(const Activity *activity);
	std::string nextNonce();

	std::string applicationId;
	Activity currentActivity;
	long long startTimestamp{0};
	uint64_t nextServiceMs{0};
	uint64_t nextRefreshMs{0};
	uint64_t nextReconnectMs{0};
	uint64_t nonceCounter{0};
	bool enabled{false};
	bool connected{false};
	bool hasActivity{false};
	bool unavailableLogged{false};
	bool failureLogged{false};
	bool updateLogged{false};

#ifdef WIN32
	void *connection{nullptr};
#elif defined(LINUX) || defined(MACOSX)
	int connection{-1};
#endif
};

extern DiscordPresence discordPresence;
