#include "dcc_delegate.h"
#include <esp_timer.h>

#include "definitions.h"
#include <lvgl.h>
#include <stdio.h>

// lv_msg_send must be called from the LVGL task. These helpers schedule the
// send via lv_async_call so that delegate callbacks (which fire from wifi_loop_task)
// are safe to use.
namespace {

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

} // namespace

void DCCEXProtocolDelegateImpl::receivedServerVersion(int major, int minor, int patch) {
  printf("Server Version: %d.%d.%d\n", major, minor, patch);
}

void DCCEXProtocolDelegateImpl::receivedMessage(char *message) { printf("Broadcast Message: %s\n", message); }

void DCCEXProtocolDelegateImpl::receivedRosterList() {
  printf("Roster list received.\n");
  async_send(MSG_DCC_ROSTER_LIST_RECEIVED);
}

void DCCEXProtocolDelegateImpl::receivedTurnoutList() {
  printf("Turnout list received.\n");
  async_send(MSG_DCC_TURNOUT_LIST_RECEIVED);
}

void DCCEXProtocolDelegateImpl::receivedRouteList() {
  printf("Route list received.\n");
  async_send(MSG_DCC_ROUTE_LIST_RECEIVED);
}

void DCCEXProtocolDelegateImpl::receivedTurntableList() {
  printf("Turntable list received.\n");
  async_send(MSG_DCC_TURNTABLE_LIST_RECEIVED);
}

void DCCEXProtocolDelegateImpl::receivedLocoUpdate(DCCExController::Loco *loco) {
  printf("Loco Update: Address=%d\n", loco->getAddress());
}

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

void DCCEXProtocolDelegateImpl::receivedTrackPower(DCCExController::TrackPower state) {
  printf("Track Power State: %d\n", state);
}

void DCCEXProtocolDelegateImpl::receivedIndividualTrackPower(DCCExController::TrackPower state, int track) {
  printf("Individual Track Power: Track=%d, State=%d\n", track, state);
}

void DCCEXProtocolDelegateImpl::receivedTrackType(char track, DCCExController::TrackManagerMode type, int address) {
  printf("Track Type: Track=%c, Type=%d, Address=%d\n", track, type, address);
}

void DCCEXProtocolDelegateImpl::receivedTurnoutAction(int turnoutId, bool thrown) {
  printf("Turnout Action: ID=%d, Thrown=%s\n", turnoutId, thrown ? "true" : "false");
  turnoutActionData.turnoutId = turnoutId;
  turnoutActionData.thrown = thrown;
  async_send_turnout(MSG_DCC_TURNOUT_CHANGED, turnoutActionData);
}

void DCCEXProtocolDelegateImpl::receivedTurntableAction(int turntableId, int position, bool moving) {
  printf("Turntable Action: ID=%d, Position=%d, Moving=%s\n", turntableId, position, moving ? "true" : "false");
}

void DCCEXProtocolDelegateImpl::receivedReadLoco(int address) { printf("Read Loco Address: %d\n", address); }

void DCCEXProtocolDelegateImpl::receivedValidateCV(int cv, int value) {
  printf("Validate CV: CV=%d, Value=%d\n", cv, value);
}

void DCCEXProtocolDelegateImpl::receivedValidateCVBit(int cv, int bit, int value) {
  printf("Validate CV Bit: CV=%d, Bit=%d, Value=%d\n", cv, bit, value);
}

void DCCEXProtocolDelegateImpl::receivedWriteLoco(int address) { printf("Write Loco Address: %d\n", address); }

void DCCEXProtocolDelegateImpl::receivedWriteCV(int cv, int value) { printf("Write CV: CV=%d, Value=%d\n", cv, value); }

void DCCEXProtocolDelegateImpl::receivedScreenUpdate(int screen, int row, char *message) {
  printf("Screen Update: Screen=%d, Row=%d, Message=%s\n", screen, row, message);
}

uint32_t DCCEXProtocolDelegateImpl::millis() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000); // Convert microseconds to milliseconds
}
