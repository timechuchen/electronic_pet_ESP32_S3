#pragma once

// All on-device text lives here; tools/build_fonts.cjs subsets these characters.
#define UI_TITLE "兰洛"
#define UI_OFFLINE "离线陪伴 · 点按聊天"
#define UI_LIVE "已连接 · 点按聊天"
#define UI_COMPANION "慢慢来，我一直在"
#define UI_WAKE_HINT "轻触屏幕，叫醒我"
#define UI_PAUSED_HINT "已暂停 · 轻触唤醒"
#define UI_IDLE "点按互动 · 长按休息"
#define UI_REST "休息一下"
#define UI_RESUME "继续陪你"
#define UI_CHAT_LIVE "请在电脑上聊天"
#define UI_CHAT_OFFLINE "连接电脑即可聊天"
#define UI_CONTINUE "继续"
#define UI_PAUSE "暂停"
#define UI_PAGE_LABELS { \
    {"挥手", "街舞", "更多"}, \
    {"微笑", "眨单眼", "更多"}, \
    {"点头", "摇头", "更多"}, \
    {"跳跃", "轻舞", "更多"}, \
    {"眨眼", UI_PAUSE, "首页"} \
}
