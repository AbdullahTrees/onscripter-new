package org.umineko_project.onscripter_ru;

import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.libsdl.app.SDLActivity;

/**
 * Translates Android touch gestures into the input the engine actually consumes
 * on this platform.
 *
 * Plain taps already work without this class: the engine groups finger events
 * itself and reads the count as a mouse button, so a two-finger tap is a
 * right-click on its own. What it cannot do under SDL3 is gestures -- the
 * multi-gesture branch of ONScripter::touchEvent opens with
 * "#if defined(ONS_USE_SDL3) return false;", leaving two-finger scrolling and
 * every three-finger swipe as dead code. This class adds those, and a
 * long-press alternative to the two-finger right-click.
 *
 * Which events reach the engine at all is the part that constrains the design.
 * Its dispatch switch in Event.cpp is split:
 *
 *     #if defined(IOS) || defined(DROID)
 *         case SDL_FINGERDOWN / SDL_FINGERUP  -> touchEvent
 *     #else
 *         case SDL_MOUSEBUTTONDOWN / UP       -> mousePressEvent
 *         case SDL_MOUSEWHEEL                 -> mouseScrollEvent
 *     #endif
 *
 * On Android the mouse-button and wheel cases are not compiled. Synthesising
 * mouse buttons therefore does nothing at all, however well-formed they are.
 * SDL_MOUSEMOTION sits outside that #if and is handled on every platform.
 *
 * So this class drives both paths deliberately:
 *
 *   - Buttons go through onNativeTouch, as a count of simultaneous fingers.
 *     The engine decides which button a touch means by grouping finger events
 *     itself: the first sets the id to 1, and any further event within
 *     MAX_TOUCH_TAP_TIMESPAN (80 ms) increments it, giving left, right and
 *     middle. Whatever id SDL supplies is overwritten, so a right-click is two
 *     finger-ups in quick succession rather than a particular id.
 *
 *   - Position goes through onNativeMouse as ACTION_MOVE. mouseButtonDecision
 *     resolves a left click against `hoveringButton`, which only mouseMoveEvent
 *     updates, so a click without a preceding motion lands on no button at all.
 *
 * Right-click is not optional in this game: it opens the menus, and the
 * file-verification screen accepts nothing else -- its left-click exit is
 * commented out in the script.
 */
final class TouchInput {
    private static final String C = "TouchInput";

    // Mirrors the ACTION_* constants in SDL's Android backend.
    private static final int SDL_ACTION_DOWN = 0;
    private static final int SDL_ACTION_UP = 1;
    private static final int SDL_ACTION_MOVE = 2;

    // Simultaneous fingers the engine must see to read a button. Its grouping
    // window is 80 ms, so these are emitted back to back.
    private static final int FINGERS_LEFT = 1;
    private static final int FINGERS_RIGHT = 2;

    /** Held still for this long with one finger is a right-click. */
    private static final long LONG_PRESS_MS = 400;
    /** A multi-finger touch shorter than this, that did not move, is a tap. */
    private static final long TAP_MS = 300;
    /** Pixels of two-finger travel per scroll step. */
    private static final float SCROLL_STEP_PX = 64f;

    private enum State {
        IDLE,
        /** One finger down, not yet committed to a tap or a drag. */
        ONE_PENDING,
        /** One finger down and moving. */
        ONE_DRAG,
        TWO,
        THREE,
        /** Gesture resolved; ignore the rest until all fingers lift. */
        SPENT,
    }

    /** Supplies the SDL surface geometry, since events arrive in screen space. */
    interface SurfaceMapper {
        float toSurfaceX(float rawX);

        float toSurfaceY(float rawY);

        int surfaceWidth();

        int surfaceHeight();
    }

    private final SurfaceMapper mapper;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final int touchSlop;
    private final int swipeThreshold;

    private State state = State.IDLE;
    private int touchDeviceId;

    private float startX, startY;
    private long startTime;
    private float lastScrollY;
    private boolean multiMoved;
    /** Whether startX/startY have been set by a real touch yet. */
    private boolean hasPoint;

    private final Runnable longPress = () -> {
        if (state != State.ONE_PENDING) {
            return;
        }
        Diag.i(C, "gesture: long press -> right click");
        click(FINGERS_RIGHT, startX, startY);
        state = State.SPENT;
    };

    TouchInput(ViewConfiguration config, SurfaceMapper mapper) {
        this.mapper = mapper;
        this.touchSlop = config.getScaledTouchSlop();
        // Deliberately larger than the tap slop: three fingers drift more than
        // one, and a swipe should not fire while the user is still deciding.
        this.swipeThreshold = config.getScaledTouchSlop() * 6;
    }

    /**
     * Feeds one event through the recogniser.
     *
     * @return true when the event was handled and must not reach SDL's own
     * touch listener, which would otherwise deliver the same finger twice.
     */
    boolean onTouch(MotionEvent event) {
        if (SDLActivity.mBrokenLibraries
                || SDLActivity.mCurrentNativeState != SDLActivity.NativeState.RESUMED) {
            // The engine is not listening yet; leave the event alone.
            return false;
        }

        touchDeviceId = event.getDeviceId();
        float x = mapper.toSurfaceX(event.getRawX());
        float y = mapper.toSurfaceY(event.getRawY());

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                beginSingle(x, y, event.getEventTime());
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                beginMulti(event);
                break;
            case MotionEvent.ACTION_MOVE:
                move(event, x, y);
                break;
            case MotionEvent.ACTION_POINTER_UP:
                resolveMulti(event);
                break;
            case MotionEvent.ACTION_UP:
                endSingle(x, y);
                break;
            case MotionEvent.ACTION_CANCEL:
                cancel();
                break;
            default:
                break;
        }
        return true;
    }

    /** Drops any gesture in progress. Call when the activity loses the surface. */
    void reset() {
        cancel();
    }

    /**
     * Plays a system Back press into the game as a right-click.
     *
     * Right-click is what the script already treats as "back", and it is
     * context-sensitive without anyone having to ask what screen is showing.
     * mouseButtonDecision only acts on one when
     * `(rmode_flag && WAIT_TEXT_MODE) || WAIT_BUTTON_MODE | WAIT_RCLICK_MODE`,
     * so the same press opens the menu during the novel, steps out of a
     * submenu, and does nothing at the title screen or mid-effect.
     *
     * Escape is not a substitute. It reaches the same buttonState of -1 but by
     * a different route, bypassing that gate, and from a scene it jumps
     * straight to the title screen.
     *
     * @return true when the press was delivered to the engine. False means the
     * engine was not listening; the caller decides what Back should mean then.
     */
    boolean systemBack() {
        if (SDLActivity.mBrokenLibraries
                || SDLActivity.mCurrentNativeState != SDLActivity.NativeState.RESUMED) {
            return false;
        }

        // A Back press arriving mid-gesture would otherwise interleave with it.
        cancel();

        float x = hasPoint ? startX : mapper.surfaceWidth() / 2f;
        float y = hasPoint ? startY : mapper.surfaceHeight() / 2f;
        Diag.i(C, "system back -> right click");
        click(FINGERS_RIGHT, x, y);
        return true;
    }

    // --- gesture phases -----------------------------------------------------

    private void beginSingle(float x, float y, long eventTime) {
        cancelLongPress();
        state = State.ONE_PENDING;
        startX = x;
        startY = y;
        hasPoint = true;
        startTime = eventTime;
        multiMoved = false;
        // Move the cursor immediately so anything under the finger is hovered
        // before a tap resolves.
        motion(x, y);
        handler.postDelayed(longPress, LONG_PRESS_MS);
    }

    private void beginMulti(MotionEvent event) {
        cancelLongPress();

        int count = event.getPointerCount();
        state = count >= 3 ? State.THREE : State.TWO;
        float[] centre = centroid(event);
        startX = centre[0];
        startY = centre[1];
        hasPoint = true;
        startTime = event.getEventTime();
        lastScrollY = centre[1];
        multiMoved = false;
    }

    private void move(MotionEvent event, float x, float y) {
        switch (state) {
            case ONE_PENDING:
                if (Math.hypot(x - startX, y - startY) <= touchSlop) {
                    return;
                }
                cancelLongPress();
                state = State.ONE_DRAG;
                motion(x, y);
                break;

            case ONE_DRAG:
                motion(x, y);
                break;

            case TWO: {
                float[] centre = centroid(event);
                if (Math.abs(centre[1] - startY) > touchSlop) {
                    multiMoved = true;
                }
                int steps = (int) ((centre[1] - lastScrollY) / SCROLL_STEP_PX);
                if (steps != 0) {
                    scroll(steps);
                    lastScrollY += steps * SCROLL_STEP_PX;
                }
                break;
            }

            case THREE: {
                float[] centre = centroid(event);
                if (Math.abs(centre[0] - startX) > touchSlop
                        || Math.abs(centre[1] - startY) > touchSlop) {
                    multiMoved = true;
                }
                break;
            }

            default:
                break;
        }
    }

    private void resolveMulti(MotionEvent event) {
        long held = event.getEventTime() - startTime;

        if (state == State.TWO && !multiMoved && held < TAP_MS) {
            Diag.i(C, "gesture: two-finger tap -> right click");
            click(FINGERS_RIGHT, startX, startY);
        } else if (state == State.THREE && multiMoved) {
            float[] centre = centroid(event);
            swipe(centre[0] - startX, centre[1] - startY);
        }

        if (state != State.IDLE) {
            // One gesture per touch sequence. Without this, lifting fingers one
            // at a time re-enters TWO and fires a spurious right click.
            state = State.SPENT;
        }
    }

    private void endSingle(float x, float y) {
        cancelLongPress();
        if (state == State.ONE_PENDING) {
            Diag.i(C, "gesture: tap -> left click");
            click(FINGERS_LEFT, x, y);
        }
        state = State.IDLE;
    }

    private void cancel() {
        cancelLongPress();
        state = State.IDLE;
    }

    private void swipe(float dx, float dy) {
        if (Math.abs(dx) < swipeThreshold && Math.abs(dy) < swipeThreshold) {
            return;
        }

        // The directions the SDL2 handler used, expressed with real keys.
        // ONS_SCANCODE_SKIP and ONS_SCANCODE_MUTE are synthetic values above
        // SDL_NUM_SCANCODES that no keyboard can produce; the engine accepts
        // Alt+S and Alt+M for the same actions.
        if (Math.abs(dx) > Math.abs(dy)) {
            if (dx > 0) {
                Diag.i(C, "gesture: three-finger swipe right -> skip");
                key(KeyEvent.KEYCODE_S, true);
            } else {
                Diag.i(C, "gesture: three-finger swipe left -> auto");
                key(KeyEvent.KEYCODE_A, false);
            }
        } else {
            if (dy > 0) {
                Diag.i(C, "gesture: three-finger swipe down -> backlog");
                key(KeyEvent.KEYCODE_TAB, false);
            } else {
                Diag.i(C, "gesture: three-finger swipe up -> mute");
                key(KeyEvent.KEYCODE_M, true);
            }
        }
    }

    // --- event emission -----------------------------------------------------

    /**
     * A click is `fingers` simultaneous touches down and up at the same spot,
     * preceded by a motion so the engine knows what is under the cursor.
     *
     * The releases are what carry the button: the engine only acts on a
     * right-click for a finger-up, and its grouping counts the events that
     * arrive inside one 80 ms window.
     */
    private void click(int fingers, float x, float y) {
        motion(x, y);
        for (int i = 0; i < fingers; i++) {
            finger(i, SDL_ACTION_DOWN, x, y);
        }
        for (int i = 0; i < fingers; i++) {
            finger(i, SDL_ACTION_UP, x, y);
        }
    }

    private void finger(int pointerId, int action, float x, float y) {
        int w = mapper.surfaceWidth();
        int h = mapper.surfaceHeight();
        if (w <= 0 || h <= 0) {
            return;
        }
        // SDL expects normalised coordinates for touch, unlike mouse events.
        SDLActivity.onNativeTouch(touchDeviceId, pointerId, action, x / w, y / h, 1.0f);
    }

    private void motion(float x, float y) {
        SDLActivity.onNativeMouse(0, SDL_ACTION_MOVE, x, y, false);
    }

    /**
     * Scrolling cannot use the wheel: SDL_MOUSEWHEEL is inside the same #if that
     * excludes mouse buttons on Android, and the engine's gesture path is
     * disabled under SDL3. Arrow keys are the remaining route.
     */
    private void scroll(int steps) {
        int keyCode = steps > 0 ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN;
        for (int i = 0; i < Math.abs(steps); i++) {
            key(keyCode, false);
        }
    }

    private void key(int keyCode, boolean withAlt) {
        if (withAlt) {
            SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_ALT_LEFT);
        }
        SDLActivity.onNativeKeyDown(keyCode);
        SDLActivity.onNativeKeyUp(keyCode);
        if (withAlt) {
            SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_ALT_LEFT);
        }
    }

    // --- helpers ------------------------------------------------------------

    private void cancelLongPress() {
        handler.removeCallbacks(longPress);
    }

    /**
     * Average of the active pointers, skipping the one being lifted so a release
     * does not drag the centre sideways just as the gesture resolves.
     */
    private float[] centroid(MotionEvent event) {
        int skip = event.getActionMasked() == MotionEvent.ACTION_POINTER_UP
                ? event.getActionIndex() : -1;
        float sx = 0, sy = 0;
        int n = 0;
        for (int i = 0; i < event.getPointerCount(); i++) {
            if (i == skip) {
                continue;
            }
            sx += mapper.toSurfaceX(event.getRawX(i));
            sy += mapper.toSurfaceY(event.getRawY(i));
            n++;
        }
        if (n == 0) {
            return new float[] { startX, startY };
        }
        return new float[] { sx / n, sy / n };
    }
}
