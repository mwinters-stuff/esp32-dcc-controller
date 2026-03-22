#ifndef _LIST_ITEM_BASE_H
#define _LIST_ITEM_BASE_H

#include "Screen.h"
#include <string>

namespace display {

class IListItem {
public:
  virtual ~IListItem() = default;

  virtual lv_obj_t *getLvObj() const = 0;
  virtual std::string getDisplayName() const = 0;
  virtual lv_image_dsc_t const *getImage() const = 0;
  virtual int getId() const = 0;

  lv_obj_t *parentObj = nullptr;
  size_t index = 0;
};

};
#endif // _LIST_ITEM_BASE_H