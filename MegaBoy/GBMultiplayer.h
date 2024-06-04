#pragma once
#include <atomic>
#include <thread>
#include <cstdint>
#include "GBCore.h"

class GBMultiplayer
{
public:
	GBMultiplayer(GBCore& gbCore) : gbCore(gbCore)
	{ }

	~GBMultiplayer();

	std::atomic<bool> stopConnection { false };

	bool host();
	bool connect(const char* ipAddress);

	void testMessage();
private:
	bool eNetInitialized { false };
	GBCore& gbCore;
	std::thread multiplayerThread;

	static constexpr uint16_t MEGABOY_UDP_PORT = 56789;

	typedef class _ENetPeer ENetPeer;
	typedef class _ENetHost ENetHost;
	void connectionHandle();

	ENetHost* userHost { nullptr };
	ENetPeer* connectedPeer { nullptr };
};