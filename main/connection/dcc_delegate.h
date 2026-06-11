#ifndef _DCC_DELEGATE_H
#define _DCC_DELEGATE_H

#include "definitions.h"
#include <DCCEXProtocol.h>
#include <stdio.h>

struct TurnoutActionData {
  int turnoutId;
  bool thrown;
};

struct TurntableActionData {
  int turntableId;
  int position;
  bool moving;
};

class DCCEXProtocolDelegateImpl : public DCCEXProtocolDelegate {
public:
  DCCEXProtocolDelegateImpl() {}
  virtual ~DCCEXProtocolDelegateImpl() {}

  void receivedServerVersion(int major, int minor, int patch) override;

  void receivedMessage(const char *message) override;

  void receivedRosterList() override;

  void receivedTurnoutList() override;

  void receivedRouteList() override;

  void receivedTurntableList() override;

  void receivedLocoUpdate(Loco *loco) override;

  void receivedLocoBroadcast(int address, int speed, Direction direction, int functionMap) override;

  void receivedTrackPower(TrackPower state) override;

  void receivedIndividualTrackPower(TrackPower state, int track) override;

  void receivedTrackType(char track, TrackManagerMode type, int address) override;

  void receivedTurnoutAction(int turnoutId, bool thrown) override;

  void receivedTurntableAction(int turntableId, int position, bool moving) override;

  void receivedReadLoco(int address) override;

  void receivedValidateCV(int cv, int value) override;

  void receivedValidateCVBit(int cv, int bit, int value) override;

  void receivedWriteLoco(int address) override;

  void receivedWriteCV(int cv, int value) override;

  void receivedScreenUpdate(int screen, int row, const char *message) override;
};

#endif