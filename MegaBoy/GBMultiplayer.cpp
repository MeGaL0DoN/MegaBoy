#define ENET_IMPLEMENTATION
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <enet/enet.h>
#include "GBMultiplayer.h"

GBMultiplayer::~GBMultiplayer()
{
	stopConnection = true; 
	if (multiplayerThread.joinable()) multiplayerThread.join();
	if (eNetInitialized) enet_deinitialize();
}

bool GBMultiplayer::connect(const char* ipAddress)
{
	if (!eNetInitialized && enet_initialize() != 0)
		return false;

	eNetInitialized = true;

	ENetAddress address { 0 };
	address.port = MEGABOY_UDP_PORT;
	enet_address_set_host(&address, ipAddress);

	userHost = enet_host_create(NULL, 1, 1, 0, 0);

	if (userHost == NULL)
		return false;

	auto peer = enet_host_connect(userHost, &address, 2, 0);

	if (peer == NULL)
		return false;

	multiplayerThread = std::thread { &GBMultiplayer::connectionHandle, this};
	return true;
}

void GBMultiplayer::testMessage()
{
	char data[5];
	auto packet = enet_packet_create(data, 5, ENET_PACKET_FLAG_RELIABLE);

	enet_peer_send(connectedPeer, 0, packet);
	enet_host_flush(userHost);
}

void GBMultiplayer::connectionHandle()
{
	ENetEvent event;

	while (!stopConnection)
	{
		if (enet_host_service(userHost, &event, 10) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		{
			connectedPeer = event.peer;
			break;
		}
	}

	if (stopConnection)
	{
		stopConnection = false;
		return;
	}

	std::cout << "Connection successfull! \n";

	while (enet_host_service(userHost, &event, 100000) > 0)
	{
		switch (event.type) 
		{
		case ENET_EVENT_TYPE_RECEIVE:
			std::cout << "Receive data! \n";
			enet_packet_destroy(event.packet);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:

			break;
		}
	}

	enet_host_destroy(userHost);
}

bool GBMultiplayer::host()
{
	if (!eNetInitialized && enet_initialize() != 0)
		return false;

	eNetInitialized = true;

	ENetAddress address { 0 };
	address.port = MEGABOY_UDP_PORT;
	userHost = enet_host_create(&address, 1, 1, 0, 0);

	if (userHost == NULL)
		return false;

	multiplayerThread = std::thread { &GBMultiplayer::connectionHandle, this };
	return true;
}