package org.umineko_project.onscripter_ru;

import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;

public class ONSActivity extends SDLActivity {
    private static final String TAG = "ONSActivity";
    private static final String PROVIDER_DIR = "ONScripter-RU";

    @Override
    protected String[] getLibraries() {
        // SDL3 is statically linked into libmain.so, so there is no
        // libSDL3.so to dlopen. Loading the SDL default list would throw
        // UnsatisfiedLinkError before "main" is ever loaded, leaving every
        // native method unresolved.
        return new String[] { "main" };
    }

    @Override
    protected String getMainFunction() {
        // The engine declares a plain `int main(...)` in Engine/Core/Loader.cpp
        // and never includes SDL_main.h, so SDL2's `#define main SDL_main`
        // shim is not in effect. libmain.so exports "main", not "SDL_main".
        return "main";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        configureScopedStorage();
    }

    /** App-scoped directory holding the game data, or null if unavailable. */
    private File resolveGameDir() {
        File baseDir = getExternalFilesDir(null);
        if (baseDir == null) {
            baseDir = getFilesDir();
        }
        if (baseDir == null) {
            Log.e(TAG, "Unable to resolve app-scoped storage directory");
            return null;
        }
        return new File(baseDir, PROVIDER_DIR);
    }

    @Override
    protected String[] getArguments() {
        // SDL3's nativeSetenv writes to SDL's own environment object, not to
        // libc's environ, so the engine's std::getenv("EXTERNAL_STORAGE") in
        // Support/FileIO.cpp never sees it and falls back to /sdcard. Pass the
        // scoped path explicitly instead; --root is authoritative.
        File dir = resolveGameDir();
        if (dir == null) {
            return new String[0];
        }
        Log.i(TAG, "game root: " + dir.getAbsolutePath());
        return new String[] { "--root", dir.getAbsolutePath() };
    }

    private void configureScopedStorage() {
        File launchDir = resolveGameDir();
        if (launchDir == null) {
            return;
        }
        File baseDir = launchDir.getParentFile();
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
