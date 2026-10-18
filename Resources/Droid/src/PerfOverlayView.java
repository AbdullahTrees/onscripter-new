package org.umineko_project.onscripter_ru;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

/**
 * The performance counter, drawn as an Android view above the game surface.
 *
 * On the desktop the engine blits the panel into the scene. That has two costs
 * here. The panel lands in canvas space, so on a display whose aspect does not
 * match the script it sits on the artwork while the letterbox band beside it
 * stays empty. And because something on top of the scene changes every frame,
 * the engine has to redraw the whole scene every frame while the panel is up --
 * so turning the counter on changes the frame times it reports. A view
 * composited by Android costs the engine nothing per frame and lives in surface
 * space, where the letterbox band is.
 *
 * The engine keeps the sampling, the classification and the formatting; this
 * class only draws what nativePerfText and nativePerfFrames hand it. Layout
 * mirrors the engine panel in Event.cpp: headline, frame graph, four body rows,
 * then the section label in its colour.
 *
 * There is no switch on this side. While the counter is off nativePerfText
 * returns null and the view stays gone, polling slowly in case the engine turns
 * it on.
 */
final class PerfOverlayView extends View {
    /** Matches the engine's own rebuild cadence, FPS_DISPLAY_UPDATE_INTERVAL_NS. */
    private static final long POLL_SHOWN_MS = 250;
    /** Only has to notice the counter being turned on. */
    private static final long POLL_HIDDEN_MS = 1000;

    /** PERF_HISTORY_SAMPLES in Event.cpp. Extra capacity would just go unused. */
    private static final int HISTORY_SAMPLES = 150;
    /** The longest row the engine formats is 46 characters. */
    private static final int WIDEST_ROW_CHARS = 48;
    private static final int BODY_ROWS = 4;

    private static final int COLOR_BACKGROUND = 0xB4000000;
    private static final int COLOR_GRAPH_BACKGROUND = 0xD2101010;
    private static final int COLOR_BUDGET_LINE = 0x5AFFFFFF;
    private static final int COLOR_INSIDE_BUDGET = 0xEB50DC6E;
    private static final int COLOR_NEAR_BUDGET = 0xEBEBC83C;
    private static final int COLOR_OVER_BUDGET = 0xEBE65046;

    private final Paint headlinePaint;
    private final Paint bodyPaint;
    private final Paint sectionPaint;
    private final Paint fillPaint = new Paint();

    private final float padding;
    private final float rowGap;
    private final float graphHeight;
    private final float marginPx;

    private final float[] frames = new float[HISTORY_SAMPLES];
    private final float[] target = new float[1];
    private int frameCount;
    private String[] lines = new String[0];

    private boolean polling;
    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            refresh();
            postDelayed(this, getVisibility() == VISIBLE ? POLL_SHOWN_MS : POLL_HIDDEN_MS);
        }
    };

    PerfOverlayView(Context context) {
        super(context);
        headlinePaint = textPaint(sp(14));
        bodyPaint = textPaint(sp(11));
        sectionPaint = textPaint(sp(13));
        padding = dp(8);
        rowGap = dp(6);
        graphHeight = dp(28);
        marginPx = dp(12);

        // A debug panel must never take a touch from the game beneath it.
        setClickable(false);
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        setVisibility(GONE);
    }

    /** Pinned to the top-left of the window, which on a letterboxed display is the band. */
    ViewGroup.LayoutParams layoutParams() {
        RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.addRule(RelativeLayout.ALIGN_PARENT_TOP);
        params.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
        params.setMargins((int) marginPx, (int) marginPx, 0, 0);
        return params;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updatePolling();
    }

    @Override
    protected void onDetachedFromWindow() {
        // Android clears its attached state after this callback returns.
        polling = false;
        removeCallbacks(poll);
        super.onDetachedFromWindow();
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        updatePolling();
    }

    /** Polls only while the window is on screen; the engine is idle otherwise anyway. */
    private void updatePolling() {
        boolean wanted = isAttachedToWindow() && getWindowVisibility() == VISIBLE;
        if (wanted == polling) {
            return;
        }
        polling = wanted;
        removeCallbacks(poll);
        if (wanted) {
            post(poll);
        }
    }

    private void refresh() {
        String text = nativePerfText();
        if (text == null) {
            if (getVisibility() != GONE) {
                setVisibility(GONE);
            }
            return;
        }
        lines = text.split("\n", -1);
        sectionPaint.setColor(0xFF000000 | nativePerfSectionColor());
        frameCount = nativePerfFrames(frames, target);
        if (getVisibility() != VISIBLE) {
            setVisibility(VISIBLE);
        }
        invalidate();
    }

    /**
     * Sized from the monospace advance rather than the text, so the panel holds
     * still as the digits change instead of resizing four times a second.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        float width = 2 * padding + bodyPaint.measureText("0") * WIDEST_ROW_CHARS;
        float height = padding
                + lineHeight(headlinePaint) + rowGap
                + graphHeight + rowGap
                + BODY_ROWS * lineHeight(bodyPaint) + rowGap
                + lineHeight(sectionPaint)
                + padding;
        setMeasuredDimension(resolveSize((int) Math.ceil(width), widthMeasureSpec),
                resolveSize((int) Math.ceil(height), heightMeasureSpec));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float width = getWidth();
        fillPaint.setColor(COLOR_BACKGROUND);
        canvas.drawRect(0, 0, width, getHeight(), fillPaint);

        float y = padding;
        y = drawRow(canvas, 0, headlinePaint, y) + rowGap;
        y = drawGraph(canvas, y, width - 2 * padding) + rowGap;
        for (int i = 1; i <= BODY_ROWS; i++) {
            y = drawRow(canvas, i, bodyPaint, y);
        }
        drawRow(canvas, BODY_ROWS + 1, sectionPaint, y + rowGap);
    }

    /** Draws lines[index], if the engine sent one, and returns the y below it. */
    private float drawRow(Canvas canvas, int index, Paint paint, float top) {
        if (index < lines.length && !lines[index].isEmpty()) {
            canvas.drawText(lines[index], padding, top - paint.ascent(), paint);
        }
        return top + lineHeight(paint);
    }

    /**
     * One bar per frame, oldest at the left. Full scale is twice the budget, so
     * the midline is the budget and a bar above it is a frame that missed --
     * the same reading as the engine-drawn graph.
     */
    private float drawGraph(Canvas canvas, float top, float graphWidth) {
        fillPaint.setColor(COLOR_GRAPH_BACKGROUND);
        canvas.drawRect(padding, top, padding + graphWidth, top + graphHeight, fillPaint);

        float targetMs = target[0];
        if (targetMs > 0) {
            float fullScaleMs = targetMs * 2;
            float barWidth = graphWidth / HISTORY_SAMPLES;
            float bottom = top + graphHeight;
            for (int i = 0; i < frameCount; i++) {
                float ms = frames[i];
                if (ms <= 0) {
                    continue;
                }
                if (ms > targetMs * 1.5f) {
                    fillPaint.setColor(COLOR_OVER_BUDGET);
                } else if (ms > targetMs * 1.05f) {
                    fillPaint.setColor(COLOR_NEAR_BUDGET);
                } else {
                    fillPaint.setColor(COLOR_INSIDE_BUDGET);
                }
                float barHeight = Math.min(1f, ms / fullScaleMs) * graphHeight;
                float left = padding + i * barWidth;
                canvas.drawRect(left, bottom - barHeight, left + barWidth, bottom, fillPaint);
            }
        }

        fillPaint.setColor(COLOR_BUDGET_LINE);
        float mid = top + graphHeight / 2;
        canvas.drawRect(padding, mid, padding + graphWidth, mid + Math.max(1f, dp(1) / 2), fillPaint);
        return top + graphHeight;
    }

    private static float lineHeight(Paint paint) {
        return paint.descent() - paint.ascent();
    }

    private Paint textPaint(float size) {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        // The system monospace face (DroidSansMono) ships no bold, so this is a
        // synthetic bold -- the weight FusionHUD draws with, and easier to read
        // over the scene than the regular cut.
        paint.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
        paint.setTextSize(size);
        paint.setColor(0xFFFFFFFF);
        return paint;
    }

    private float dp(float value) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                getResources().getDisplayMetrics());
    }

    private float sp(float value) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, value,
                getResources().getDisplayMetrics());
    }

    private static native String nativePerfText();

    private static native int nativePerfSectionColor();

    private static native int nativePerfFrames(float[] frames, float[] target);
}
