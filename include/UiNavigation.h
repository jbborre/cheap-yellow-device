#pragma once
// Shared focus for menus. Gameplay keeps its independent held-button controls.
namespace UiNavigation {
int context = -1, index = 0;
const TouchButton *target = nullptr;
bool visible = false;
bool update(int nextContext, const TouchButton *const *items, int count, uint8_t pressed) {
  bool redraw=context!=nextContext || (!visible && count>0);
  if (context!=nextContext) { context=nextContext; index=0; }
  if (count && ((pressed & 3)==1 || (pressed & 3)==2)) {
    index=(index+count+((pressed & 1) ? -1 : 1))%count;
    redraw=true;
  }
  visible=count>0;
  target=count ? items[index] : nullptr;
  return redraw;
}
void draw() {
  if (!visible || !target) return;
  gameFrame.drawRoundRect(target->x,target->y,target->w,target->h,10,COLOR_GOLD);
  gameFrame.drawRoundRect(target->x+1,target->y+1,target->w-2,target->h-2,9,COLOR_GOLD);
}
}
