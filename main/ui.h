#pragma once

/* 页面编号 */
#define UI_PAGE_CLOCK   0
#define UI_PAGE_WEATHER 1
#define UI_PAGE_BILI    2
#define UI_PAGE_MSG     3
#define UI_PAGE_COUNT   4

/* 把 g_data 渲染到帧缓冲（不负责推屏） */
void ui_render(int page);
