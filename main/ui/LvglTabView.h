#pragma once
#include <lvgl.h>
#include <string>
#include <vector>

namespace ui
{
class LvglTabView
{
public:
    LvglTabView(lv_obj_t *parent, lv_align_t align, int x_ofs, int y_ofs, int width, int height)
    {
        tabview_ = lv_tabview_create(parent, LV_DIR_TOP, 40);
        lv_obj_set_size(tabview_, width, height);
        lv_obj_align(tabview_, align, x_ofs, y_ofs);
    }

    lv_obj_t *addTab(const std::string &name)
    {
        return lv_tabview_add_tab(tabview_, name.c_str());
    }

    lv_obj_t *lvObj() const { return tabview_; }

private:
    lv_obj_t *tabview_;
};
}