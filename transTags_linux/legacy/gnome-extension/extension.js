import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const OPACITY_STEP = 26;
const MIN_OPACITY = 25;
const FULL_OPACITY = 255;

const BINDINGS = [
    'transparency-up',
    'transparency-down',
    'unlock-click-through',
    'pin-window',
    'center-window',
];

export default class TransTagsExtension extends Extension {
    enable() {
        this._settings = this.getSettings();
        this._states = new Map();
        this._recentClickThrough = [];

        Main.wm.addKeybinding(
            'transparency-up',
            this._settings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.ALL,
            () => this._adjustOpacity(true)
        );
        Main.wm.addKeybinding(
            'transparency-down',
            this._settings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.ALL,
            () => this._adjustOpacity(false)
        );
        Main.wm.addKeybinding(
            'unlock-click-through',
            this._settings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.ALL,
            () => this._unlockClickThrough()
        );
        Main.wm.addKeybinding(
            'pin-window',
            this._settings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.ALL,
            () => this._togglePin()
        );
        Main.wm.addKeybinding(
            'center-window',
            this._settings,
            Meta.KeyBindingFlags.NONE,
            Shell.ActionMode.ALL,
            () => this._centerWindow()
        );
    }

    disable() {
        for (const binding of BINDINGS)
            Main.wm.removeKeybinding(binding);

        if (this._states) {
            for (const [window, state] of this._states.entries()) {
                this._setClickThrough(window, false);
                if (state.pinned)
                    this._setPinned(window, false);
                if (state.signalId) {
                    try {
                        window.disconnect(state.signalId);
                    } catch (error) {
                        // The window can already be unmanaged while the extension is disabled.
                    }
                }
            }
        }

        this._recentClickThrough = null;
        this._states = null;
        this._settings = null;
    }

    _focusedWindow() {
        const window = global.display.get_focus_window?.() ?? global.display.focus_window;
        return this._isUsableWindow(window) ? window : null;
    }

    _isUsableWindow(window) {
        if (!window)
            return false;

        if (window.is_skip_taskbar?.())
            return false;

        const windowType = window.get_window_type?.();
        return windowType === undefined ||
            windowType === Meta.WindowType.NORMAL ||
            windowType === Meta.WindowType.DIALOG ||
            windowType === Meta.WindowType.MODAL_DIALOG;
    }

    _actorFor(window) {
        if (!window)
            return null;

        try {
            return window.get_compositor_private?.() ?? null;
        } catch (error) {
            logError(error, 'transTags: failed to get window actor');
            return null;
        }
    }

    _stateFor(window) {
        let state = this._states.get(window);
        if (state)
            return state;

        const actor = this._actorFor(window);
        state = {
            opacity: actor?.opacity ?? FULL_OPACITY,
            clickThrough: false,
            pinned: false,
            signalId: 0,
        };

        try {
            state.signalId = window.connect('unmanaged', () => this._forgetWindow(window));
        } catch (error) {
            state.signalId = 0;
        }

        this._states.set(window, state);
        return state;
    }

    _forgetWindow(window) {
        const state = this._states?.get(window);
        if (!state)
            return;

        if (state.signalId) {
            try {
                window.disconnect(state.signalId);
            } catch (error) {
                // The unmanaged signal can fire during window teardown.
            }
        }

        this._states.delete(window);
        this._recentClickThrough = this._recentClickThrough?.filter(item => item !== window) ?? [];
    }

    _rememberClickThrough(window) {
        this._recentClickThrough = this._recentClickThrough.filter(item => item !== window);
        this._recentClickThrough.push(window);
    }

    _forgetClickThrough(window) {
        this._recentClickThrough = this._recentClickThrough.filter(item => item !== window);
    }

    _lastClickThroughWindow() {
        for (let i = this._recentClickThrough.length - 1; i >= 0; i--) {
            const window = this._recentClickThrough[i];
            const state = this._states.get(window);
            if (state?.clickThrough && this._isUsableWindow(window))
                return window;

            this._recentClickThrough.splice(i, 1);
        }

        return null;
    }

    _pinTargetWindow() {
        return this._lastClickThroughWindow() ?? this._focusedWindow();
    }

    _setActorOpacity(actor, opacity) {
        if (!actor)
            return;

        if (typeof actor.set_opacity === 'function')
            actor.set_opacity(opacity);
        else
            actor.opacity = opacity;
    }

    _setActorReactive(actor, reactive) {
        if (!actor)
            return false;

        if (typeof actor.set_reactive === 'function') {
            actor.set_reactive(reactive);
            return true;
        }

        if ('reactive' in actor) {
            actor.reactive = reactive;
            return true;
        }

        return false;
    }

    _adjustOpacity(moreTransparent) {
        const window = this._focusedWindow();
        if (!window)
            return;

        const actor = this._actorFor(window);
        if (!actor)
            return;

        const state = this._stateFor(window);
        const currentOpacity = actor.opacity ?? state.opacity ?? FULL_OPACITY;
        let opacity = moreTransparent
            ? Math.max(MIN_OPACITY, currentOpacity - OPACITY_STEP)
            : Math.min(FULL_OPACITY, currentOpacity + OPACITY_STEP);

        this._setActorOpacity(actor, opacity);
        state.opacity = opacity;
        this._setClickThrough(window, opacity < FULL_OPACITY);

        this._notify(`${this._windowTitle(window)}: ${Math.round(opacity * 100 / FULL_OPACITY)}%`);
    }

    _setClickThrough(window, enabled) {
        const actor = this._actorFor(window);
        const state = this._stateFor(window);

        if (enabled) {
            if (this._setActorReactive(actor, false)) {
                state.clickThrough = true;
                this._rememberClickThrough(window);
            }
        } else {
            this._setActorReactive(actor, true);
            state.clickThrough = false;
            this._forgetClickThrough(window);
        }
    }

    _unlockClickThrough() {
        const window = this._lastClickThroughWindow();
        if (!window)
            return;

        this._setClickThrough(window, false);
        this._notify(`${this._windowTitle(window)}: click-through off`);
    }

    _togglePin() {
        const window = this._pinTargetWindow();
        if (!window)
            return;

        const state = this._stateFor(window);
        this._setPinned(window, !state.pinned);
        this._notify(`${this._windowTitle(window)}: always-on-top ${state.pinned ? 'on' : 'off'}`);
    }

    _setPinned(window, enabled) {
        const state = this._stateFor(window);

        if (enabled) {
            if (typeof window.make_above === 'function')
                window.make_above();
            else if (typeof window.raise === 'function')
                window.raise();
        } else if (typeof window.unmake_above === 'function') {
            window.unmake_above();
        }

        state.pinned = enabled;
    }

    _centerWindow() {
        const window = this._focusedWindow();
        if (!window)
            return;

        const frame = window.get_frame_rect();
        const monitor = window.get_monitor?.() ?? global.display.get_current_monitor();
        const workArea = Main.layoutManager.getWorkAreaForMonitor(monitor);
        const x = Math.round(workArea.x + (workArea.width - frame.width) / 2);
        const y = Math.round(workArea.y + (workArea.height - frame.height) / 2);

        if (typeof window.move_frame === 'function')
            window.move_frame(true, x, y);
        else if (typeof window.move_resize_frame === 'function')
            window.move_resize_frame(true, x, y, frame.width, frame.height);

        this._notify(`${this._windowTitle(window)}: centered`);
    }

    _windowTitle(window) {
        try {
            return window.get_title?.() || 'Window';
        } catch (error) {
            return 'Window';
        }
    }

    _notify(message) {
        Main.notify('transTags', message);
    }
}
