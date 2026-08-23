package org.umineko_project.onscripter_ru;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.Arrays;

public class ONSActivity extends SDLActivity {
    private static final String C = "ONSActivity";

    @Override
    protected String[] getLibraries() {
        // SDL3 is statically linked into libmain.so, so there is no
        // libSDL3.so to dlopen. Loading the SDL default list would throw
        // UnsatisfiedLinkError before "main" is ever loaded, leaving every
        // native method unresolved.
        String[] libraries = new String[] { "main" };
        Diag.i(C, "getLibraries -> " + Arrays.toString(libraries));
        return libraries;
    }

    @Override
    protected String getMainFunction() {
        // The engine declares a plain `int main(...)` in Engine/Core/Loader.cpp
        // and never includes SDL_main.h, so SDL2's `#define main SDL_main`
        // shim is not in effect. libmain.so exports "main", not "SDL_main".
        Diag.i(C, "getMainFunction -> main");
        return "main";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Diag.i(C, "onCreate");
        super.onCreate(savedInstanceState);
        configureScopedStorage();
    }

    @Override
    protected void onDestroy() {
        // The engine calls exit() on a fatal error, so this is often the last
        // Java line before the process goes away.
        Diag.i(C, "onDestroy");
        super.onDestroy();
    }

    /**
     * Directory holding the game data: the folder chosen in SetupActivity when one
     * is set and still readable, otherwise the app-scoped location.
     */
    private File resolveGameDir() {
        return GameStorage.resolveRoot(this);
    }

    @Override
    protected String[] getArguments() {
        File dir = resolveGameDir();
        if (dir == null) {
            Diag.e(C, "no game directory could be resolved, launching with no arguments");
            return new String[0];
        }

        Diag.logDirectory(C, "launch dir", dir);

        // --root is authoritative: the engine refuses to let a later path
        // override it, so the game folder cannot be second-guessed by a config
        // file or by whatever getLaunchDir() derives from the environment.
        //
        // Saves are deliberately not passed with --save. Overriding save_path
        // suppresses lookupSavePath(), which is what applies the engine's own
        // layout, and left two SaveData directories in the game folder. Setting
        // EXTERNAL_STORAGE below is enough to keep the engine's structure inside
        // the chosen folder.
        String[] arguments = new String[] { "--root", dir.getAbsolutePath() };
        Diag.i(C, "handing off to engine, argv " + Arrays.toString(arguments));
        return arguments;
    }

    private void configureScopedStorage() {
        File launchDir = resolveGameDir();
        if (launchDir == null) {
            Diag.e(C, "unable to resolve any storage directory");
            return;
        }
        // Only ever create the app-scoped fallback. A folder the user picked is
        // theirs; if it has gone away, resolveRoot has already stopped returning it.
        if (!launchDir.isDirectory() && launchDir.equals(GameStorage.getScopedRoot(this))) {
            boolean created = launchDir.mkdirs();
            Diag.i(C, "created app-scoped launch dir: " + created);
            if (!created) {
                Diag.e(C, "unable to create launch directory: " + launchDir.getAbsolutePath());
            }
        }

        // SDL3's nativeSetenv calls POSIX setenv() directly -- deliberately, not
        // SDL_setenv -- so these do reach the engine's std::getenv.
        //
        // getLaunchDir() appends "ONScripter-RU" to EXTERNAL_STORAGE, and
        // getStorageDir() hangs "SaveData" off that. Pointing it at the game
        // folder rather than the folder above keeps everything the engine
        // creates inside the directory the user chose, in the engine's own
        // layout: <game folder>/ONScripter-RU/SaveData/<game id>/.
        String basePath = launchDir.getAbsolutePath();
        Diag.i(C, "nativeSetenv EXTERNAL_STORAGE=" + basePath);
        nativeSetenv("EXTERNAL_STORAGE", basePath);
        nativeSetenv("SECONDARY_STORAGE", basePath);
        nativeSetenv("EXTERNAL_SDCARD_STORAGE", basePath);
        nativeSetenv("HOME", basePath);
        nativeSetenv("ONS_SCOPED_STORAGE", launchDir.getAbsolutePath());
    }
}
