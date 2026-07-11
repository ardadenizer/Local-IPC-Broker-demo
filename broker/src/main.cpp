#include "BrokerRuntime.hpp"
#include "ipc/SocketConfig.hpp"

#include <iostream>
#include <string>

int main()
{
  std::cout.setf(std::ios::unitbuf);
  std::cout << "[Broker] is starting...\n";

  BrokerRuntime runtime{std::string{ipc::BrokerSocketPath}};
  return runtime.run();
}