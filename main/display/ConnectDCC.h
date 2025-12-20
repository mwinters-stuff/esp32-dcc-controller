#pragma once
#include "Screen.h"
#include <memory>

#include "DCCConnectListItem.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include "ui/LvglListView.h"
#include "ui/LvglTabView.h"

namespace display {
class ConnectDCCScreen : public Screen, public std::enable_shared_from_this<ConnectDCCScreen> {
public:
  static std::shared_ptr<ConnectDCCScreen> instance() {
    static std::shared_ptr<ConnectDCCScreen> s;
    if (!s)
      s.reset(new ConnectDCCScreen());
    return s;
  }
  ConnectDCCScreen(const ConnectDCCScreen &) = delete;
  ConnectDCCScreen &operator=(const ConnectDCCScreen &) = delete;
  ~ConnectDCCScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void connectToDCCServer(std::shared_ptr<DCCConnectListItem> dccItem);

  void refreshMdnsList();
  void refreshSavedList();
  void resetMsgHandlers();

private:
  std::vector<std::shared_ptr<DCCConnectListItem>> detectedListItems;
  std::vector<std::shared_ptr<DCCConnectListItem>> savedListItems;

  void *mdns_added_sub_ = nullptr;
  void *mdns_changed_sub_ = nullptr;

  std::unique_ptr<ui::LvglLabel> lbl_title_;
  std::unique_ptr<ui::LvglTabView> tab_view_;
  std::unique_ptr<ui::LvglListView> list_auto_;
  std::unique_ptr<ui::LvglListView> list_saved_;
  std::unique_ptr<ui::LvglButton> btn_back_;
  std::unique_ptr<ui::LvglButton> btn_save_;
  std::unique_ptr<ui::LvglButton> btn_connect_;
protected:
  ConnectDCCScreen() = default;

};
} // namespace display
