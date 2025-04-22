#pragma once
#include <Windows.h>
#include <functional>

class HotkeyManager {
public:
    static HotkeyManager& get() {
        static HotkeyManager instance;
        return instance;
    }

    void install() {
        if (!m_hook) {
            m_hook = SetWindowsHookExA(WH_KEYBOARD_LL, HookProc, nullptr, 0);
        }
    }

    void uninstall() {
        if (m_hook) {
            UnhookWindowsHookEx(m_hook);
            m_hook = nullptr;
        }
    }

    void setCallback(std::function<void()> cb) {
        m_callback = std::move(cb);
    }

private:
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam) {
        if (code == HC_ACTION && wParam == WM_KEYDOWN) {
            auto info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            // ✅ Change VK_MENU (Alt) to VK_CONTROL (Ctrl)
            if (info->vkCode == VK_SPACE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                if (get().m_callback) get().m_callback();
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    std::function<void()> m_callback;
    HHOOK m_hook = nullptr;
};
