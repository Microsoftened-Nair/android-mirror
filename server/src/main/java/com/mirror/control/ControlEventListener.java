package com.mirror.control;

/**
 * Callback interface defining event handlers for inbound control channel commands.
 */
public interface ControlEventListener {
    void onTouchEvent(int action, long pointerId, int x, int y, int canvasWidth, int canvasHeight, int pressure, int buttons);
    void onKeyEvent(int action, int keyCode, int metaState);
    void onScrollEvent(int x, int y, int hScroll, int vScroll);
    void onSetClipboard(String text, boolean pasteNow);
    void onSetScreenPower(boolean on);
    void onRequestKeyframe();
    void onHeartbeatPing(int pingId);
}
