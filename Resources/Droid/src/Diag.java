package org.umineko_project.onscripter_ru;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.util.Arrays;

/**
 * Logging for the Java half of the app.
 *
 * Everything here logs under one tag, {@link #TAG}, with the component named in
 * the message. The engine logs under "ONScripter-RU" (FileIO::log ->
 * __android_log_vprint). Two tags, one per side of the JNI boundary, so a single
 * filter shows the whole startup and it is never ambiguous which half produced a
 * line:
 *
 *   adb logcat -s ONSJava:V ONScripter-RU:V SDL:V
 *
 * Native crashes do not appear on either tag. A SIGSEGV inside the engine is
 * reported by the "libc"/"DEBUG" tags as "Fatal signal 11", followed by a
 * tombstone; the Java handler installed here cannot see those.
 */
final class Diag {
    /** Every Java-side log line in this app uses this tag. */
    static final String TAG = "ONSJava";

    private Diag() {
    }

    static void i(String component, String message) {
        Log.i(TAG, component + ": " + message);
    }

    static void w(String component, String message) {
        Log.w(TAG, component + ": " + message);
    }

    static void w(String component, String message, Throwable t) {
        Log.w(TAG, component + ": " + message, t);
    }

    static void e(String component, String message) {
        Log.e(TAG, component + ": " + message);
    }

    static void e(String component, String message, Throwable t) {
        Log.e(TAG, component + ": " + message, t);
    }

    /**
     * Records what a bug report always needs and nobody ever includes: which
     * build, which device, which ABI. The ABI matters because the APK ships
     * arm64-v8a and x86_64 only, and a mismatch shows up as a dlopen failure
     * with no obvious cause.
     */
    static void logEnvironment(Context context) {
        String version = "unknown";
        try {
            PackageInfo info = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0);
            version = info.versionName + " (" + info.getLongVersionCode() + ")";
        } catch (PackageManager.NameNotFoundException e) {
            w("Diag", "Could not read own package info", e);
        }

        i("Diag", "app " + context.getPackageName() + " " + version);
        i("Diag", "device " + Build.MANUFACTURER + " " + Build.MODEL
                + ", Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")");
        i("Diag", "abis " + Arrays.toString(Build.SUPPORTED_ABIS));
    }

    /** Logs a directory with the facts that decide whether the engine can use it. */
    static void logDirectory(String component, String label, File dir) {
        if (dir == null) {
            i(component, label + " = null");
            return;
        }
        String[] entries = dir.list();
        i(component, label + " = " + dir.getAbsolutePath()
                + " [exists=" + dir.exists()
                + " dir=" + dir.isDirectory()
                + " canRead=" + dir.canRead()
                + " canWrite=" + dir.canWrite()
                + " entries=" + (entries == null ? "denied" : String.valueOf(entries.length))
                + "]");
    }

    /**
     * Logs uncaught Java exceptions before the runtime tears the process down.
     *
     * Without this an early crash is easy to misread as an engine fault, because
     * the visible symptom is the same: the window disappears. A line on this tag
     * says the Java side died and the engine was never reached.
     *
     * The default handler is still invoked afterwards, so the usual
     * AndroidRuntime trace and crash dialog are unaffected.
     */
    static void installCrashHandler(Context context) {
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            try {
                e("Diag", "uncaught exception on thread \"" + thread.getName() + "\"", error);
                CrashReport.writeForException(context, thread, error);
                context.startActivity(CrashActivity.intentFor(context));
            } catch (Throwable ignored) {
                // Never let diagnostics replace the original failure.
            }

            // Deliberately not delegating to the platform handler. It would put
            // its own "keeps stopping" dialog on top of the crash screen we just
            // launched. Ending the process here leaves only our screen, which
            // lives in a separate process and so survives this.
            //
            // The report is already on disk, so nothing is lost by skipping the
            // system's own record. Native crashes never reach here anyway --
            // those are found afterwards through ApplicationExitInfo.
            android.os.Process.killProcess(android.os.Process.myPid());
            System.exit(10);
        });
    }
}
