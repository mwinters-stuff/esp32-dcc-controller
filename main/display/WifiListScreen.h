#pragma once
#include <memory>
#include <vector>
#include <utility>
#include "screen.h"

#include "ui/LvglLabel.h"
#include "ui/LvglListView.h"
#include "ui/WifiListItem.h"
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


    void show(lv_obj_t* parent = nullptr) override;
    void cleanUp() override;

   // Populate list (clears old items)
    void setWifiList(const std::vector<std::pair<std::string,int>>& wifiList,
                     WifiListItem::TapCallback globalTap = nullptr);

    // Optionally return the list object to load screen
    lv_obj_t* listObj() const { return list_view_->lvObj(); }
    lv_obj_t* titleObj() const { return lbl_title_->lvObj(); }


    WifiListScreen(const WifiListScreen&) = delete;
    WifiListScreen& operator=(const WifiListScreen&) = delete;

protected:
    WifiListScreen() = default;


private:
    std::unique_ptr<LvglLabel> lbl_title_;
    std::unique_ptr<LvglListView> list_view_;
    std::vector<std::shared_ptr<WifiListItem>> items_;
};

} // namespace ui
