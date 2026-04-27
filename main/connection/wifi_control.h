#pragma once

#ifndef _WIFI_CONTROL_H
#define _WIFI_CONTROL_H

#include <DCCEXProtocol.h>
#include <lwip/tcp.h>

#include <memory>

#include "ESP_Millis.h"
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
  static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
  static void tcp_connect_err_callback(void *arg, err_t err);
  connection_state currentConnectionState = NOT_CONNECTED;
  uint64_t lastGetListsMs = 0;
  DCCExController::DCCMillis *dccMillis;
  volatile bool connectCallbackDone_ = false;
  volatile bool connectCallbackSuccess_ = false;
  volatile err_t connectCallbackErr_ = ERR_OK;

  struct ConnectTaskArgs {
    WifiControl *self;
    std::string server_ip;
    uint16_t port;
  };
  static void connect_task(void *arg);
};
}; // namespace utilities

#endif