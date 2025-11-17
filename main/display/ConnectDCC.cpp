#include "ConnectDCC.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include "ui/LvglTabView.h"
#include "ui/LvglListView.h"
#include "Screen.h"
#include "utilities/WifiHandler.h"
#include <memory>
#include <vector>
#include "definitions.h"

namespace display
{
  std::shared_ptr<DCCConnectListItem> DCCConnectListItem::currentListItem;
  lv_obj_t *DCCConnectListItem::currentButton = NULL;

  std::shared_ptr<ConnectDCCScreen> ConnectDCCScreen::instance()
  {
    static std::shared_ptr<ConnectDCCScreen> s;
    if (!s)
      s.reset(new ConnectDCCScreen());
    return s;
  }

  void ConnectDCCScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen)
  {
    Screen::show(parent, parentScreen);

    // Title
    lbl_title_ = std::make_unique<ui::LvglLabel>(lvObj_, "Connect DCC", LV_ALIGN_TOP_MID, 0, 8);
    lbl_title_->setStyle("label.title");

    // Calculate tabview position and size
    int title_height = 40; // Adjust if your title uses a different height
    int button_height = 52; // Button height + margin
    int tabview_y = title_height + 8; // 8px offset from top
    int tabview_height = LV_VER_RES - tabview_y - button_height - 16; // 16px for bottom margin

    // Tabs - fill space between title and buttons, no padding
    tab_view_ = std::make_unique<ui::LvglTabView>(lvObj_, LV_ALIGN_TOP_LEFT, 0, tabview_y, LV_PCT(100), tabview_height);

    auto tab_auto_ = tab_view_->addTab("Auto Detect");
    auto tab_saved_ = tab_view_->addTab("Saved");
    lv_obj_set_style_pad_all(tab_auto_, 0, LV_PART_MAIN); // Remove all padding
    lv_obj_set_style_pad_all(tab_saved_, 0, LV_PART_MAIN); // Remove all padding

    // List for Auto Detect
    list_auto_ = std::make_unique<ui::LvglListView>(tab_auto_, 0, 0, LV_PCT(100), tabview_height - lv_obj_get_height(tab_auto_));
    // TODO: Populate list_auto_ with mDNS discovered _withrottle devices

    // List for Saved
    list_saved_ = std::make_unique<ui::LvglListView>(tab_saved_, 0, 0, LV_PCT(100), tabview_height - lv_obj_get_height(tab_auto_));
    // TODO: Populate list_saved_ with saved DCC connections

    // Bottom Buttons
    btn_back_ = std::make_unique<ui::LvglButton>(lvObj_, "Back", [this](lv_event_t *e)
    {
      if (lv_event_get_code(e) == LV_EVENT_CLICKED)
      {
        if (auto screen = parentScreen_.lock())
          screen->show();
      }
    });
    btn_back_->setStyle("button.secondary");
    btn_back_->setAlignment(LV_ALIGN_BOTTOM_LEFT, 8, -12);
    btn_back_->setSize(100, 40);

    btn_save_ = std::make_unique<ui::LvglButton>(lvObj_, "Save", [this](lv_event_t *e)
    {
      if (lv_event_get_code(e) == LV_EVENT_CLICKED)
      {
        if (DCCConnectListItem::currentListItem)
        {
          auto listItem = DCCConnectListItem::currentListItem;
          ESP_LOGI("DCC_CONNECT_SCREEN", "Connect button pressed on %s",  listItem->getText().c_str());
          // handle save action.
          auto savedListItem = std::make_shared<DCCConnectListItem>(list_saved_->lvObj(), listItem->device());
          savedListItems.push_back(savedListItem);
        }
                                                  // Handle connect action
      }
    });
    btn_save_->setStyle("button.primary");
    btn_save_->setAlignment(LV_ALIGN_BOTTOM_MID, 0, -12);
    btn_save_->setSize(100, 40);

    btn_connect_ = std::make_unique<ui::LvglButton>(lvObj_, "Connect", [this](lv_event_t *e)
    {
      if (lv_event_get_code(e) == LV_EVENT_CLICKED)
      {
        // TODO: Connect to selected DCC connection
      }
    });
    btn_connect_->setStyle("button.primary");
    btn_connect_->setAlignment(LV_ALIGN_BOTTOM_RIGHT, -8, -12);
    btn_connect_->setSize(100, 40);

    lv_msg_subscribe(MSG_MDNS_DEVICE_ADDED, [](void * s, lv_msg_t * msg)
    { 
      ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(msg->user_data);
      self->refreshMdnsList();
    }, this);
    lv_msg_subscribe(MSG_MDNS_DEVICE_CHANGED, [](void * s, lv_msg_t * msg)
    { 
      ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(msg->user_data);
      self->refreshMdnsList();
    }, this);

    refreshMdnsList();
  }

  void ConnectDCCScreen::refreshMdnsList()
  {
      auto wifiHandler = utilities::WifiHandler::instance();
      auto devices = wifiHandler->getWithrottleDevices();

      for (const auto &device : devices)
      {
          bool found = false;
          for (auto &item : detectedListItems)
          {
              if (item->matches(device)) // Assume DCCConnectListItem has a matches() method for identity
              {
                  found = true;
                  if (!item->isSame(device)) // Assume isSame() checks if details differ
                  {
                      item->update(device); // Update the item with new device info
                  }
                  break;
              }
          }
          if (!found)
          {
              auto listItem = std::make_shared<DCCConnectListItem>(list_auto_->lvObj(), device);
              detectedListItems.push_back(listItem);
          }
      }
  }

  void ConnectDCCScreen::refreshSavedList(){
    for (const auto &item : savedListItems)
    {
        // Assume item is already added to list_saved_ during save action
        // If additional refresh logic is needed, implement here
    }
  }

  void ConnectDCCScreen::cleanUp()
  {
    lbl_title_.reset();
    tab_view_.reset();
    list_auto_.reset();
    list_saved_.reset();
    btn_back_.reset();
    btn_save_.reset();
    btn_connect_.reset();
    Screen::cleanUp();
  }
} // namespace display