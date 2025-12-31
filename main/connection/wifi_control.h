#pragma once

#ifndef _WIFI_CONTROL_H
#define _WIFI_CONTROL_H

#include <DCCEXProtocol.h>
#include <lwip/tcp.h>

#include <memory>

#include "dcc_delegate.h"
#include "wifi_connection.h"

namespace utilities {

class WifiControl {
private:
  std::shared_ptr<DCCExController::DCCEXProtocol> dccExProtocol;
  DCCEXProtocolDelegateImpl dccDelegate;
  TCPSocketStream *stream = nullptr;
  LoggingStream *logStream = nullptr;
  uint8_t *ip_address;

  // Private constructor to prevent instantiation from outside
  // This ensures that the class can only be instantiated through the
  // initInstance method and provides a single instance of WifiControl
  // throughout the application. This is a common pattern for implementing the
  // Singleton design pattern in C++.
  explicit WifiControl(const WifiControl &) = delete;
  WifiControl &operator=(const WifiControl &) = delete;
  WifiControl();

public:
  enum connection_state {
    NOT_CONNECTED,
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
  };
  virtual ~WifiControl() {};

  static std::shared_ptr<WifiControl> instance() {
    static std::shared_ptr<WifiControl> s;
    if (!s)
      s.reset(new WifiControl());
    return s;
  }
  void init();
  bool connect();
  connection_state connectionState() const { return currentConnectionState; }

  void failError(err_t err);
  void connectToServer(const char *server_ip, uint16_t port);
  void loop();
  void startConnectToServer(const char *server_ip, uint16_t port);
  void disconnect();

  std::shared_ptr<DCCExController::DCCEXProtocol> dccProtocol() { return dccExProtocol; };

private:
  connection_state currentConnectionState = NOT_CONNECTED;

  struct ConnectTaskArgs {
    WifiControl *self;
    std::string server_ip;
    uint16_t port;
  };
  static void connect_task(void *arg);
};
}; // namespace utilities

#endif