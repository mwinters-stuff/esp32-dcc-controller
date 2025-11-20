
#include <lvgl.h>
#include "LvglListItem.h"

namespace ui
{
  lv_obj_t *LvglListItem::currentButton = nullptr;
  std::shared_ptr<LvglListItem> LvglListItem::currentItem = nullptr;

} // namespace ui