package org.umineko_project.onscripter_ru;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Environment;
import android.provider.DocumentsContract;

import java.io.File;

/**
 * Resolves the directory the engine is launched against.
 *
 * The engine reaches the filesystem through plain POSIX calls -- FileIO::accessFile
 * is a stat(2) and the readers are fopen(3). It has no notion of content:// URIs, so
 * the Storage Access Framework alone cannot feed it: a folder the user grants through
 * ACTION_OPEN_DOCUMENT_TREE is reachable by DocumentsContract but still fails stat().
 *
 * The only way to give the engine an arbitrary user-chosen folder is therefore
 * MANAGE_EXTERNAL_STORAGE, which restores POSIX access to shared storage.
 *
 * The two mechanisms do different jobs and both are needed: the picker names the
 * folder, the permission makes it readable. Persisting the URI grant does not help
 * the engine -- that grant exists only in the DocumentsProvider layer and puts
 * nothing in the kernel's view of the path.
 */
final class GameStorage {
    private static final String C = "GameStorage";

    private static final String PREFS = "storage";
    private static final String KEY_ROOT = "game_root";

    /** Subdirectory used for the app-scoped fallback locations. */
    static final String PROVIDER_DIR = "ONScripter-RU";

    private GameStorage() {
    }

    /**
     * Whether the app holds "All files access". Without it the engine cannot stat
     * anything outside its own scoped directories, whatever the user picked.
     */
    static boolean hasAllFilesAccess() {
        return Environment.isExternalStorageManager();
    }

    /** The directory the user chose, or null if unset or no longer usable. */
    static File getConfiguredRoot(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String stored = prefs.getString(KEY_ROOT, null);
        if (stored == null) {
            return null;
        }
        File dir = new File(stored);
        if (isUsable(dir)) {
            return dir;
        }
        // Configured but unreachable: the folder was deleted, the card was
        // removed, or all-files access was revoked after it was chosen.
        Diag.w(C, "configured root is no longer usable: " + stored);
        Diag.logDirectory(C, "configured root", dir);
        return null;
    }

    static void setConfiguredRoot(Context context, File dir) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                .edit()
                .putString(KEY_ROOT, dir.getAbsolutePath())
                .apply();
        Diag.i(C, "stored game root " + dir.getAbsolutePath());
    }

    /**
     * App-scoped fallback. Always reachable without any permission, which makes it
     * the safe default, but it is awkward for the user to fill: file managers hide
     * Android/data on current Android versions, so in practice it wants adb push.
     */
    static File getScopedRoot(Context context) {
        File base = context.getExternalFilesDir(null);
        if (base == null) {
            base = context.getFilesDir();
        }
        if (base == null) {
            Diag.e(C, "unable to resolve app-scoped storage directory");
            return null;
        }
        return new File(base, PROVIDER_DIR);
    }

    /**
     * The directory to launch against: the user's choice when it is usable,
     * otherwise the app-scoped location.
     */
    static File resolveRoot(Context context) {
        File configured = getConfiguredRoot(context);
        if (configured != null) {
            Diag.i(C, "resolveRoot -> configured " + configured.getAbsolutePath());
            return configured;
        }
        File scoped = getScopedRoot(context);
        Diag.i(C, "resolveRoot -> app-scoped fallback " + scoped);
        return scoped;
    }

    /**
     * A real POSIX readability check, not an existence check. canRead() alone
     * answers from the permission bits and still returns true for directories
     * that list() cannot open, which is exactly the shared-storage case this
     * class exists to detect.
     */
    static boolean isUsable(File dir) {
        return dir != null && dir.isDirectory() && dir.canRead() && dir.list() != null;
    }

    /**
     * Logs what the app can actually see on shared storage.
     *
     * Worth keeping: run-as cannot answer this question. It runs in a restricted
     * mount namespace that never receives the pass-through mount, so it reports
     * shared storage as unreadable even when the app itself can read it. The only
     * reliable probe is one that runs inside the app process.
     */
    static void logAccessState() {
        File external = Environment.getExternalStorageDirectory();
        String[] entries = external == null ? null : external.list();
        Diag.i(C, "all-files access=" + hasAllFilesAccess()
                + " external=" + external
                + " listed=" + (entries == null ? "denied" : entries.length + " entries"));
    }

    /**
     * Converts a tree URI from ACTION_OPEN_DOCUMENT_TREE into a filesystem path.
     *
     * Document ids from the external-storage provider are "<volume>:<relative path>",
     * where the volume is "primary" for built-in storage and a UUID such as
     * "1D03-2E0F" for a card. Anything else -- Drive, a cloud provider, a synthetic
     * Downloads id -- has no filesystem path at all and is rejected rather than
     * guessed at, because the engine cannot open what it cannot stat.
     */
    static File resolveTreeUri(Uri treeUri) {
        if (treeUri == null) {
            return null;
        }

        Diag.i(C, "picker returned " + treeUri);

        String authority = treeUri.getAuthority();
        if (!"com.android.externalstorage.documents".equals(authority)) {
            Diag.w(C, "unsupported document provider: " + authority);
            return null;
        }

        String documentId;
        try {
            documentId = DocumentsContract.getTreeDocumentId(treeUri);
        } catch (IllegalArgumentException e) {
            Diag.w(C, "not a tree URI: " + treeUri, e);
            return null;
        }
        if (documentId == null) {
            return null;
        }

        String[] parts = documentId.split(":", 2);
        String volume = parts[0];
        String relative = parts.length > 1 ? parts[1] : "";

        File base;
        if ("primary".equalsIgnoreCase(volume)) {
            base = Environment.getExternalStorageDirectory();
        } else {
            base = new File("/storage/" + volume);
            if (!base.isDirectory()) {
                Diag.w(C, "unknown storage volume: " + volume);
                return null;
            }
        }

        File resolved = relative.isEmpty() ? base : new File(base, relative);
        Diag.i(C, "resolved " + documentId + " -> " + resolved.getAbsolutePath());
        return resolved;
    }
}
