#pragma once
#include <memory>
#include <vector>
#include <utility>
#include <vector>
#include <esp_wifi.h>

#include "Screen.h"

#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include "ui/LvglListView.h"
#include "WifiListItem.h"
#include "ui/LvglTheme.h"

namespace display {
    using namespace ui;

class WifiListScreen : public Screen, public std::enable_shared_from_this<WifiListScreen> {
public:

    static std::shared_ptr<WifiListScreen> instance() {
        static std::shared_ptr<WifiListScreen> s;
        if (!s)
            s.reset(new WifiListScreen());
        return s;
    }
    ~WifiListScreen() override = default;


    void show(lv_obj_t* parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
    void cleanUp() override;

    // Optionally return the list object to load screen
    lv_obj_t* listObj() const { return list_view_->lvObj(); }
    lv_obj_t* titleObj() const { return lbl_title_->lvObj(); }


    WifiListScreen(const WifiListScreen&) = delete;
    WifiListScreen& operator=(const WifiListScreen&) = delete;

    void scanWifi();
protected:
    WifiListScreen() = default;


private:
    lv_obj_t* _lv_obj = nullptr;
    std::unique_ptr<LvglLabel> lbl_title_;
    std::unique_ptr<LvglButton> btn_back_;
    std::unique_ptr<LvglButton> btn_connect_;
    std::unique_ptr<LvglListView> list_view_;
    std::vector<std::shared_ptr<WifiListItem>> items_;
    lv_obj_t *spinner_ = nullptr;
    void scanWifiTask();
    void populateList(const std::vector<wifi_ap_record_t> &records);
};

} // namespace display
