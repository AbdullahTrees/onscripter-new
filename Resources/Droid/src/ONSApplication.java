package org.umineko_project.onscripter_ru;

import android.app.Application;

/**
 * Installs diagnostics before any activity exists.
 *
 * This runs earlier than SetupActivity or ONSActivity, so a crash during
 * activity creation is still logged with a Java-side tag rather than only
 * appearing as an AndroidRuntime trace.
 */
public class ONSApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        Diag.installCrashHandler();
        Diag.logEnvironment(this);
    }
}
