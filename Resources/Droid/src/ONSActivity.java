package org.umineko_project.onscripter_ru;

import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;

public class ONSActivity extends SDLActivity {
    private static final String TAG = "ONSActivity";
    private static final String PROVIDER_DIR = "ONScripter-RU";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        configureScopedStorage();
    }

    private void configureScopedStorage() {
        File baseDir = getExternalFilesDir(null);
        if (baseDir == null) {
            baseDir = getFilesDir();
        }
        if (baseDir == null) {
            Log.e(TAG, "Unable to resolve app-scoped storage directory");
            return;
        }

        File launchDir = new File(baseDir, PROVIDER_DIR);
        if (!launchDir.isDirectory() && !launchDir.mkdirs()) {
            Log.e(TAG, "Unable to create launch directory: " + launchDir.getAbsolutePath());
        }

        String basePath = baseDir.getAbsolutePath();
        nativeSetenv("EXTERNAL_STORAGE", basePath);
        nativeSetenv("SECONDARY_STORAGE", basePath);
        nativeSetenv("EXTERNAL_SDCARD_STORAGE", basePath);
        nativeSetenv("HOME", basePath);
        nativeSetenv("ONS_SCOPED_STORAGE", launchDir.getAbsolutePath());
    }
}
