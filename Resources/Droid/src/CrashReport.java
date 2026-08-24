package org.umineko_project.onscripter_ru;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.List;

/**
 * Builds and stores crash reports.
 *
 * Two kinds of crash reach this class by different routes, because they behave
 * differently:
 *
 *   Java exceptions are recoverable enough to report on the spot. Diag's
 *   uncaught handler writes the report and CrashActivity is shown immediately.
 *
 *   Native crashes are not. A SIGSEGV in the engine kills the process before
 *   any Java code runs -- the earlier backgrounding crash produced a
 *   "Fatal signal 11" from libc and nothing else. Those are found afterwards,
 *   by asking ActivityManager why the previous process died, and reported on
 *   the next launch.
 */
final class CrashReport {
    private static final String C = "CrashReport";

    private static final String PREFS = "crash";
    private static final String KEY_LAST_REPORTED = "last_reported_exit_ms";

    private static final String REPORT_FILE = "last-crash.txt";

    /** Keep reports shareable: an oversized Intent extra fails at the binder. */
    private static final int MAX_REPORT_BYTES = 60 * 1024;
    private static final int LOGCAT_LINES = 200;

    private CrashReport() {
    }

    static File reportFile(Context context) {
        return new File(context.getCacheDir(), REPORT_FILE);
    }

    /** Writes a report for a Java exception that is about to kill the process. */
    static void writeForException(Context context, Thread thread, Throwable error) {
        StringBuilder out = new StringBuilder();
        appendHeader(context, out);
        out.append("Type: uncaught Java exception\n");
        out.append("Thread: ").append(thread.getName()).append("\n\n");

        StringWriter trace = new StringWriter();
        error.printStackTrace(new PrintWriter(trace));
        out.append(trace);

        appendLogcat(out);
        write(context, out.toString());
    }

    /**
     * Looks for an abnormal death of the previous process and writes a report
     * for it. Returns true when there is something new to show.
     *
     * Each exit is reported once: the timestamp of the last one shown is
     * remembered, so returning to the app does not resurrect an old crash.
     */
    static boolean captureNativeExitIfAny(Context context) {
        ActivityManager am = context.getSystemService(ActivityManager.class);
        if (am == null) {
            return false;
        }

        List<ApplicationExitInfo> exits;
        try {
            exits = am.getHistoricalProcessExitReasons(context.getPackageName(), 0, 5);
        } catch (RuntimeException e) {
            Diag.w(C, "could not read exit reasons", e);
            return false;
        }
        if (exits == null || exits.isEmpty()) {
            return false;
        }

        ApplicationExitInfo exit = exits.get(0);
        if (!isAbnormal(exit.getReason())) {
            return false;
        }

        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        if (prefs.getLong(KEY_LAST_REPORTED, 0L) >= exit.getTimestamp()) {
            return false; // already shown
        }

        StringBuilder out = new StringBuilder();
        appendHeader(context, out);
        out.append("Type: ").append(describeReason(exit.getReason())).append("\n");
        out.append("Description: ").append(exit.getDescription()).append("\n");
        out.append("Importance: ").append(exit.getImportance()).append("\n");
        out.append("Pid: ").append(exit.getPid()).append("\n\n");

        try (InputStream trace = exit.getTraceInputStream()) {
            if (trace != null) {
                out.append("--- tombstone ---\n");
                appendTombstone(trace, out);
                out.append('\n');
            }
        } catch (IOException e) {
            Diag.w(C, "could not read tombstone", e);
        }

        appendLogcat(out);
        write(context, out.toString());

        prefs.edit().putLong(KEY_LAST_REPORTED, exit.getTimestamp()).apply();
        Diag.e(C, "previous process died: " + describeReason(exit.getReason())
                + " (" + exit.getDescription() + ")");
        return true;
    }

    static String read(Context context) {
        File file = reportFile(context);
        if (!file.isFile()) {
            return "No crash report was recorded.";
        }
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "Could not read the crash report: " + e;
        }
    }

    // --- building blocks ----------------------------------------------------

    private static boolean isAbnormal(int reason) {
        return reason == ApplicationExitInfo.REASON_CRASH_NATIVE
                || reason == ApplicationExitInfo.REASON_CRASH
                || reason == ApplicationExitInfo.REASON_ANR;
    }

    private static String describeReason(int reason) {
        switch (reason) {
            case ApplicationExitInfo.REASON_CRASH_NATIVE:
                return "native crash (engine)";
            case ApplicationExitInfo.REASON_CRASH:
                return "Java crash";
            case ApplicationExitInfo.REASON_ANR:
                return "not responding";
            default:
                return "exit reason " + reason;
        }
    }

    private static void appendHeader(Context context, StringBuilder out) {
        out.append("onscripter-new crash report\n");
        out.append("App: ").append(context.getPackageName()).append('\n');
        out.append("Device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append(", Android ").append(Build.VERSION.RELEASE)
                .append(" (API ").append(Build.VERSION.SDK_INT).append(")\n");
        out.append("ABIs: ").append(Arrays.toString(Build.SUPPORTED_ABIS)).append("\n\n");
    }

    /**
     * The app's own recent log. Without READ_LOGS this returns only our process,
     * which is exactly what is wanted: the ONSJava and ONScripter-RU lines
     * leading up to the failure.
     */
    private static void appendLogcat(StringBuilder out) {
        out.append("--- recent log ---\n");
        try {
            Process process = new ProcessBuilder(
                    "logcat", "-d", "-t", String.valueOf(LOGCAT_LINES),
                    "ONSJava:V", "ONScripter-RU:V", "SDL:V", "AndroidRuntime:E", "libc:F", "*:S")
                    .redirectErrorStream(true)
                    .start();
            appendStream(process.getInputStream(), out);
            process.destroy();
        } catch (IOException e) {
            out.append("(unavailable: ").append(e).append(")\n");
        }
    }

    /**
     * Extracts the readable parts of a tombstone.
     *
     * For a native crash getTraceInputStream returns a protobuf, not text, so
     * decoding it as UTF-8 produces mojibake. Parsing it properly would mean
     * taking a protobuf dependency and carrying Android's Tombstone schema; the
     * parts worth reading -- build fingerprint, signal name, the mapped library
     * paths and the backtrace's symbol names -- are plain strings inside it, so
     * pull those out instead.
     *
     * ANR traces are already text and pass through unchanged.
     */
    private static void appendTombstone(InputStream in, StringBuilder out) throws IOException {
        StringBuilder run = new StringBuilder();
        int b;
        while ((b = in.read()) != -1 && out.length() < MAX_REPORT_BYTES) {
            if (b == '\n' || (b >= 0x20 && b < 0x7f)) {
                run.append((char) b);
                continue;
            }
            // Short runs are usually field tags and numbers rather than text.
            if (run.length() >= 4) {
                out.append(run).append('\n');
            }
            run.setLength(0);
        }
        if (run.length() >= 4) {
            out.append(run).append('\n');
        }
    }

    private static void appendStream(InputStream in, StringBuilder out) throws IOException {
        try (BufferedReader reader =
                     new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null && out.length() < MAX_REPORT_BYTES) {
                out.append(line).append('\n');
            }
        }
        if (out.length() >= MAX_REPORT_BYTES) {
            out.append("\n(truncated)\n");
        }
    }

    private static void write(Context context, String report) {
        try {
            Files.write(reportFile(context).toPath(), report.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            Diag.e(C, "could not write crash report", e);
        }
    }
}
