#pragma once
#include "LvglWidget.h"
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui
{

  class LvglContainer : public LvglWidget
  {
  public:
    enum class LayoutType
    {
      NONE,
      FLEX,
      GRID
    };

    LvglContainer(lv_obj_t *parent,
                  LayoutType layout = LayoutType::FLEX,
                  lv_flex_flow_t flow = LV_FLEX_FLOW_COLUMN,
                  lv_align_t align = LV_ALIGN_CENTER,
                  lv_coord_t x_ofs = 0,
                  lv_coord_t y_ofs = 0)
        : layout_type_(layout)
    {
      obj_ = lv_obj_create(parent);
      setAlignment(align, x_ofs, y_ofs);

      if (layout == LayoutType::FLEX)
      {
        setFlexFlow(flow);
        lv_obj_set_flex_align(obj_,
                              LV_FLEX_ALIGN_START,  // main axis
                              LV_FLEX_ALIGN_CENTER, // cross axis
                              LV_FLEX_ALIGN_CENTER  // track cross axis
        );
      }
    }

    //
    // 🔹 Child management
    //
    void addChild(std::unique_ptr<LvglWidget> child)
    {
      children_.push_back(std::move(child));
    }

    const std::vector<std::unique_ptr<LvglWidget>> &children() const { return children_; }

    //
    // 🔹 Templated add() helper
    //
    template <typename T, typename... Args>
    T &add(Args &&...args)
    {
      static_assert(std::is_base_of<LvglWidget, T>::value,
                    "T must derive from LvglWidget");

      auto widget = std::make_unique<T>(lvObj(), std::forward<Args>(args)...);
      T &ref = *widget; // Keep reference before moving
      children_.push_back(std::move(widget));
      return ref;
    }

    //
    // 🔹 Flex Layout API
    //
    void setFlexAlign(lv_flex_align_t main_place,
                      lv_flex_align_t cross_place,
                      lv_flex_align_t track_cross_place)
    {
      if (layout_type_ == LayoutType::FLEX)
        lv_obj_set_flex_align(obj_, main_place, cross_place, track_cross_place);
    }

    void setFlexFlow(lv_flex_flow_t flow)
    {
      if (layout_type_ == LayoutType::FLEX)
        lv_obj_set_flex_flow(obj_, flow);
    }

    //
    // 🔹 Grid Layout API
    //
    void setGridLayout(const std::vector<lv_coord_t> &col_dsc,
                       const std::vector<lv_coord_t> &row_dsc)
    {
      if (layout_type_ != LayoutType::GRID)
        layout_type_ = LayoutType::GRID;

      grid_cols_ = col_dsc;
      grid_rows_ = row_dsc;
      grid_cols_.push_back(LV_GRID_TEMPLATE_LAST);
      grid_rows_.push_back(LV_GRID_TEMPLATE_LAST);

      lv_obj_set_grid_dsc_array(obj_, grid_cols_.data(), grid_rows_.data());
    }

    static void setGridCell(LvglWidget &widget,
                            uint8_t col, uint8_t col_span,
                            uint8_t row, uint8_t row_span,
                            lv_grid_align_t x_align = LV_GRID_ALIGN_CENTER,
                            lv_grid_align_t y_align = LV_GRID_ALIGN_CENTER)
    {
      if (!widget.lvObj())
        return;
      lv_obj_set_grid_cell(widget.lvObj(), x_align, col, col_span, y_align, row, row_span);
    }

  protected:
    LayoutType layout_type_ = LayoutType::NONE;
    std::vector<std::unique_ptr<LvglWidget>> children_;

    std::vector<lv_coord_t> grid_cols_;
    std::vector<lv_coord_t> grid_rows_;
  };

} // namespace ui
