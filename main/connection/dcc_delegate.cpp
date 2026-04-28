/**
 * @file dcc_delegate.cpp
 * @brief DCCEXProtocol delegate implementation.
 *
 * Implements the DCCEXProtocolDelegate callbacks that DCCEXProtocol invokes
 * when it parses inbound WiThrottle packets. Each callback marshals data into
 * an lv_msg payload and schedules a send on the LVGL task via lv_async_call,
 * keeping the UI layer decoupled from the TCP/protocol thread.
 */
#include "dcc_delegate.h"
#include <esp_timer.h>

#include "definitions.h"
#include "ui/lv_msg.h"
#include <lvgl.h>
#include <stdio.h>

// lv_msg_send must be called from the LVGL task. These helpers schedule the
// send via lv_async_call so that delegate callbacks (which fire from wifi_loop_task)
// are safe to use.
namespace {

// Schedules an lv_msg_send for a bare message ID (no payload) on the LVGL
// thread.
void async_send(uint32_t msg_id) {
  auto *id = new uint32_t(msg_id);
  lv_async_call(
      [](void *arg) {
        auto *id = static_cast<uint32_t *>(arg);
        lv_msg_send(*id, nullptr);
        delete id;
      },
      id);
}

// Schedules an lv_msg_send carrying a TurnoutActionData payload on the LVGL
// thread.
void async_send_turnout(uint32_t msg_id, const TurnoutActionData &data) {
  struct Msg {
    uint32_t id;
    TurnoutActionData data;
  };
  auto *m = new Msg{msg_id, data};
  lv_async_call(
      [](void *arg) {
        auto *m = static_cast<Msg *>(arg);
        lv_msg_send(m->id, &m->data);
        delete m;
      },
      m);
}

// Schedules an lv_msg_send carrying a TurntableActionData payload on the LVGL
// thread.
void async_send_turntable(uint32_t msg_id, const TurntableActionData &data) {
  struct Msg {
    uint32_t id;
    TurntableActionData data;
  };
  auto *m = new Msg{msg_id, data};
  lv_async_call(
      [](void *arg) {
        auto *m = static_cast<Msg *>(arg);
        lv_msg_send(m->id, &m->data);
        delete m;
      },
      m);
}

// Schedules an lv_msg_send carrying a single uint8_t value on the LVGL thread.
void async_send_u8(uint32_t msg_id, uint8_t value) {
  struct Msg {
    uint32_t id;
    uint8_t value;
  };
  auto *m = new Msg{msg_id, value};
  lv_async_call(
      [](void *arg) {
        auto *m = static_cast<Msg *>(arg);
        lv_msg_send(m->id, &m->value);
        delete m;
      },
      m);
}

} // namespace

// Called when the server version string is received; fires MSG_DCC_SERVER_VERSION.
void DCCEXProtocolDelegateImpl::receivedServerVersion(int major, int minor, int patch) {
  printf("Server Version: %d.%d.%d\n", major, minor, patch);
}

// Called when the server sends a broadcast message; logged to stdout.
void DCCEXProtocolDelegateImpl::receivedMessage(const char *message) { printf("Broadcast Message: %s\n", message); }

// Called when the full roster list has been received; fires MSG_ROSTER_UPDATED.
void DCCEXProtocolDelegateImpl::receivedRosterList() {
  printf("Roster list received.\n");
  async_send(MSG_DCC_ROSTER_LIST_RECEIVED);
}

// Called when the full turnout list has been received; fires MSG_TURNOUT_LIST_UPDATED.
void DCCEXProtocolDelegateImpl::receivedTurnoutList() {
  printf("Turnout list received.\n");
  async_send(MSG_DCC_TURNOUT_LIST_RECEIVED);
}

// Called when the full route list has been received; fires MSG_ROUTE_LIST_UPDATED.
void DCCEXProtocolDelegateImpl::receivedRouteList() {
  printf("Route list received.\n");
  async_send(MSG_DCC_ROUTE_LIST_RECEIVED);
}

// Called when the full turntable list has been received; fires MSG_TURNTABLE_LIST_UPDATED.
void DCCEXProtocolDelegateImpl::receivedTurntableList() {
  printf("Turntable list received.\n");
  async_send(MSG_DCC_TURNTABLE_LIST_RECEIVED);
}

// Called when a loco update is received; fires MSG_LOCO_SPEED_UPDATED.
void DCCEXProtocolDelegateImpl::receivedLocoUpdate(DCCExController::Loco *loco) {
  printf("Loco Update: Address=%d\n", loco->getAddress());
}

// Called on a broadcast speed/direction update for a loco address;
// fires MSG_LOCO_BROADCAST_UPDATED.
void DCCEXProtocolDelegateImpl::receivedLocoBroadcast(int address, int speed, DCCExController::Direction direction,
                                                      int functionMap) {
  printf("Loco Broadcast: Address=%d, Speed=%d, Direction=%d, FunctionMap=%d\n", address, speed, direction,
         functionMap);
  // DCCExController::Loco *loco = DCCExController::Loco::getByAddress(address);
  // if (loco) {
  //   loco->setSpeed(speed);
  //   loco->setDirection(direction);
  //   loco->setFunctionStates(functionMap);
  // }
}

// Called when global track power state changes; fires MSG_TRACK_POWER_UPDATED.
void DCCEXProtocolDelegateImpl::receivedTrackPower(DCCExController::TrackPower state) {
  printf("Track Power State: %d\n", state);
  async_send_u8(MSG_DCC_TRACK_POWER_CHANGED, static_cast<uint8_t>(state));
}

// Called for per-track power updates; fires MSG_INDIVIDUAL_TRACK_POWER_UPDATED.
void DCCEXProtocolDelegateImpl::receivedIndividualTrackPower(DCCExController::TrackPower state, int track) {
  printf("Individual Track Power: Track=%d, State=%d\n", track, state);
  async_send_u8(MSG_DCC_TRACK_POWER_CHANGED, static_cast<uint8_t>(state));
}

// Called when a track's operational mode changes; logged to stdout.
void DCCEXProtocolDelegateImpl::receivedTrackType(char track, DCCExController::TrackManagerMode type, int address) {
  printf("Track Type: Track=%c, Type=%d, Address=%d\n", track, type, address);
}

// Called when a turnout throw/close event is confirmed by the server;
// fires MSG_TURNOUT_ACTION.
void DCCEXProtocolDelegateImpl::receivedTurnoutAction(int turnoutId, bool thrown) {
  printf("Turnout Action: ID=%d, Thrown=%s\n", turnoutId, thrown ? "true" : "false");
  turnoutActionData.turnoutId = turnoutId;
  turnoutActionData.thrown = thrown;
  async_send_turnout(MSG_DCC_TURNOUT_CHANGED, turnoutActionData);
}

// Called when a turntable move is initiated or completes;
// fires MSG_TURNTABLE_ACTION.
void DCCEXProtocolDelegateImpl::receivedTurntableAction(int turntableId, int position, bool moving) {
  printf("Turntable Action: ID=%d, Position=%d, Moving=%s\n", turntableId, position, moving ? "true" : "false");
  turntableActionData.turntableId = turntableId;
  turntableActionData.position = position;
  turntableActionData.moving = moving;
  async_send_turntable(MSG_DCC_TURNTABLE_CHANGED, turntableActionData);
}

// Called when a programming-track loco address read completes; logged to stdout.
void DCCEXProtocolDelegateImpl::receivedReadLoco(int address) { printf("Read Loco Address: %d\n", address); }

// Called when CV validation completes; fires MSG_CV_VALIDATED.
void DCCEXProtocolDelegateImpl::receivedValidateCV(int cv, int value) {
  printf("Validate CV: CV=%d, Value=%d\n", cv, value);
}

// Called when a CV bit validation result arrives; logged to stdout.
void DCCEXProtocolDelegateImpl::receivedValidateCVBit(int cv, int bit, int value) {
  printf("Validate CV Bit: CV=%d, Bit=%d, Value=%d\n", cv, bit, value);
}

// Called when a loco address write completes; logged to stdout.
void DCCEXProtocolDelegateImpl::receivedWriteLoco(int address) { printf("Write Loco Address: %d\n", address); }

void DCCEXProtocolDelegateImpl::receivedWriteCV(int cv, int value) { printf("Write CV: CV=%d, Value=%d\n", cv, value); }

void DCCEXProtocolDelegateImpl::receivedScreenUpdate(int screen, int row, const char *message) {
  printf("Screen Update: Screen=%d, Row=%d, Message=%s\n", screen, row, message);
}
