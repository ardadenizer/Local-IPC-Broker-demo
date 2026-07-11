#include "messaging/MessageCodec.hpp"
#include "UnixSocketServer.hpp"
#include "ipc/SocketConfig.hpp"

#include <iostream>
#include <string>
#include <unistd.h>

int main()
{
  std::cout << "[Broker] is starting...\n";

  UnixSocketServer server{std::string{ipc::BrokerSocketPath}};

  if (!server.start()) { return 1; };

  std::cout << "[broker] is listening on " << ipc::BrokerSocketPath << "\n";

  const int clientFd = server.acceptClient();

  if (clientFd == -1) {return 1; };

  std::cout << "[broker] client connected, client file descriptor: " << clientFd << "\n";

  std::string incomingJson;

  if (!server.receiveMessage(clientFd, incomingJson))
  {
    std::cerr << "[broker] failed to receive message\n";
        ::close(clientFd);
        return 1;
  }

  std::cout << "Received: " << incomingJson << std::endl;


  return 0;
}