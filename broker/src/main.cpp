#include "messaging/MessageCodec.hpp"
#include "UnixSocketServer.hpp"
#include "ipc/SocketConfig.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

bool setNonBlocking(int fd)
{
  const int currentFlags = ::fcntl(fd, F_GETFL, 0);
  if (currentFlags == -1)
  {
    return false;
  }

  return ::fcntl(fd, F_SETFL, currentFlags | O_NONBLOCK) != -1;
}

void closeClient(int clientFd, std::unordered_map<int, std::string>& clientBuffers)
{
  ::close(clientFd);
  clientBuffers.erase(clientFd);
  std::cout << "[broker] client disconnected, fd=" << clientFd << "\n";
}

void handleMessage(const messaging::Message& message)
{
  switch (message.type)
  {
    case messaging::MessageType::Publish:
      std::cout << "[broker] publish received\n"
            << "  client: " << message.clientId << '\n'
            << "  topic:  " << message.topic << '\n'
            << "  id:     " << message.messageId << '\n';
      break;

    case messaging::MessageType::Subscribe:
      std::cout << "[broker] subscribe received\n"
            << "  client: " << message.clientId << '\n'
            << "  topic:  " << message.topic << '\n';
      break;

    case messaging::MessageType::Ack:
      std::cout << "[broker] acknowledgement received\n"
            << "  client: " << message.clientId << '\n'
            << "  id:     " << message.messageId << '\n';
      break;

    case messaging::MessageType::Deliver:
      std::cerr << "[broker] client cannot send deliver messages\n";
      break;

    case messaging::MessageType::Error:
      std::cerr << "[broker] error message received from client\n";
      break;
  }
}

} // namespace

int main()
{
  std::cout.setf(std::ios::unitbuf);
  std::cout << "[Broker] is starting...\n";

  UnixSocketServer server{std::string{ipc::BrokerSocketPath}};

  if (!server.start()) 
  { 
    std::cerr << "Failed to start the socket server, terminating..." << std::endl;
    return 1; 
  }

  std::cout << "[broker] is listening on " << ipc::BrokerSocketPath << "\n";

  const int listenFd = server.listeningFd();
  if (listenFd < 0)
  {
    std::cerr << "[broker] listening fd is invalid\n";
    return 1;
  }

  if (!setNonBlocking(listenFd))
  {
    std::cerr << "[broker] failed to set non-blocking listen socket: "
              << std::strerror(errno) << '\n';
    return 1;
  }

  std::unordered_map<int, std::string> clientBuffers;

  while (true)
  {
    std::vector<pollfd> pollSet;
    pollSet.reserve(1 + clientBuffers.size());
    pollSet.push_back({listenFd, POLLIN, 0});
    for (const auto& [fd, _] : clientBuffers)
    {
      pollSet.push_back({fd, POLLIN, 0});
    }

    const int readyCount = ::poll(pollSet.data(), pollSet.size(), -1);
    if (readyCount < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      std::cerr << "[broker] poll failed: " << std::strerror(errno) << '\n';
      return 1;
    }

    if ((pollSet[0].revents & POLLIN) != 0)
    {
      while (true)
      {
        const int clientFd = server.acceptClient();
        if (clientFd < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
          {
            break;
          }
          if (errno == EINTR)
          {
            continue;
          }
          std::cerr << "[broker] accept failed: " << std::strerror(errno) << '\n';
          break;
        }

        if (!setNonBlocking(clientFd))
        {
          std::cerr << "[broker] failed to set non-blocking client socket, fd="
                    << clientFd << '\n';
          ::close(clientFd);
          continue;
        }

        clientBuffers.emplace(clientFd, std::string{});
        std::cout << "[broker] client connected, fd=" << clientFd << "\n";
      }
    }

    for (std::size_t i = 1; i < pollSet.size(); ++i)
    {
      const int clientFd = pollSet[i].fd;
      const short events = pollSet[i].revents;

      if (events == 0)
      {
        continue;
      }

      if ((events & (POLLHUP | POLLERR | POLLNVAL)) != 0)
      {
        closeClient(clientFd, clientBuffers);
        continue;
      }

      if ((events & POLLIN) == 0)
      {
        continue;
      }

      bool disconnectClient = false;
      while (!disconnectClient)
      {
        char readBuffer[512];
        const ssize_t bytesRead = ::recv(clientFd, readBuffer, sizeof(readBuffer), 0);

        if (bytesRead > 0)
        {
          auto& pending = clientBuffers[clientFd];
          pending.append(readBuffer, static_cast<std::size_t>(bytesRead));

          if (pending.size() > ipc::MaximumMessageSize)
          {
            std::cerr << "[broker] incoming message exceeds limit, fd=" << clientFd << '\n';
            disconnectClient = true;
            break;
          }

          std::size_t newlinePos = std::string::npos;
          while ((newlinePos = pending.find('\n')) != std::string::npos)
          {
            std::string incomingJson = pending.substr(0, newlinePos);
            pending.erase(0, newlinePos + 1);

            if (!incomingJson.empty() && incomingJson.back() == '\r')
            {
              incomingJson.pop_back();
            }
            if (incomingJson.empty())
            {
              continue;
            }

            std::string error;
            const auto message = messaging::MessageCodec::deserialize(incomingJson, error);
            if (!message)
            {
              std::cerr << "[broker] invalid message: " << error << '\n';
              continue;
            }

            handleMessage(*message);
          }

          continue;
        }

        if (bytesRead == 0)
        {
          disconnectClient = true;
          break;
        }

        if (errno == EINTR)
        {
          continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          break;
        }

        std::cerr << "[broker] recv failed, fd=" << clientFd
                  << " error=" << std::strerror(errno) << '\n';
        disconnectClient = true;
      }

      if (disconnectClient)
      {
        closeClient(clientFd, clientBuffers);
      }
    }
  }

  return 0;
}