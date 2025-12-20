#pragma once

#include "Screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include <memory>

namespace display {

class DCCMenu : public Screen, public std::enable_shared_from_this<DCCMenu> {
public:
  static std::shared_ptr<DCCMenu> instance() {
    static std::shared_ptr<DCCMenu> s;
    if (!s)
      s.reset(new DCCMenu());
    return s;
  }

  ~DCCMenu() override = default; 
  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  DCCMenu(const DCCMenu &) = delete;
  DCCMenu &operator=(const DCCMenu &) = delete; 
  void enableButtons(bool enableConnect);
  void disableButtons();
  void unsubscribeAll();

  std::shared_ptr<ui::LvglLabel> lbl_status;
  // std::shared_ptr<ui::LvglLabel> lbl_ip;

  void setConnectedServer(std::string ip, int port, std::string dccname);
protected:
  DCCMenu() = default;  

private:
  void *subscribe_failed;
  void *subscribe_not_saved;

  void *subscribe_dcc_roster_received;
  void *subscribe_dcc_turnout_received;
  void *subscribe_dcc_route_received;
  void *subscribe_dcc_turntable_received;

  std::string ip;
  int port;
  std::string dccname;

  std::shared_ptr<ui::LvglLabel> lbl_title;
  std::shared_ptr<ui::LvglButton> btn_roster;
  std::shared_ptr<ui::LvglButton> btn_points;
  std::shared_ptr<ui::LvglButton> btn_routes;
  std::shared_ptr<ui::LvglButton> btn_turntables;
  std::shared_ptr<ui::LvglButton> btn_close;
};

} // namespace display

