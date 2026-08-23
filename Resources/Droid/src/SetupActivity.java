package org.umineko_project.onscripter_ru;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.widget.TextView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.button.MaterialButton;

import java.io.File;

/**
 * Launcher screen. Resolves storage access, then hands off to the engine.
 *
 * This is a separate activity on purpose. SDLActivity starts the native thread from
 * its own lifecycle -- the transition to RESUMED with a ready surface is the entry
 * point to the C app -- and getArguments() is read on that thread. There is no
 * supported point inside that sequence at which to block for a permission dialog or
 * a directory picker, so the decision is made before the engine activity exists.
 *
 * Doing it here also keeps the vendored SDLActivity untouched: it stays a plain
 * android.app.Activity, and only this screen depends on AppCompat.
 */
public class SetupActivity extends AppCompatActivity {
    private static final String C = "SetupActivity";

    /** Sent by the launcher shortcut to force this screen to show. */
    static final String ACTION_RECONFIGURE = "org.umineko_project.onscripter_ru.action.RECONFIGURE";

    private TextView mStatus;
    private MaterialButton mGrantButton;
    private MaterialButton mChooseButton;
    private MaterialButton mPlayButton;

    private ActivityResultLauncher<Intent> mAllFilesLauncher;
    private ActivityResultLauncher<Uri> mFolderLauncher;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Diag.i(C, "onCreate, action=" + getIntent().getAction());

        mAllFilesLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> refresh());

        mFolderLauncher = registerForActivityResult(
                new ActivityResultContracts.OpenDocumentTree(),
                this::onFolderPicked);

        // Nothing to ask for: go straight to the game.
        if (!ACTION_RECONFIGURE.equals(getIntent().getAction()) && isConfigured()) {
            Diag.i(C, "already configured, skipping setup");
            launchEngine();
            return;
        }
        Diag.i(C, "showing setup screen");

        setContentView(R.layout.activity_setup);
        mStatus = findViewById(R.id.status);
        mGrantButton = findViewById(R.id.grant_access);
        mChooseButton = findViewById(R.id.choose_folder);
        mPlayButton = findViewById(R.id.play);

        mGrantButton.setOnClickListener(v -> requestAllFilesAccess());
        mChooseButton.setOnClickListener(v -> mFolderLauncher.launch(null));
        mPlayButton.setOnClickListener(v -> launchEngine());
    }

    @Override
    protected void onResume() {
        super.onResume();
        // All files access is granted on a Settings screen, not through a result,
        // so the state has to be re-read whenever we come back to the foreground.
        if (mStatus != null) {
            refresh();
        }
    }

    /**
     * Only a folder the user actually chose counts as configured. The app-scoped
     * fallback always exists and is always readable, so treating it as "ready"
     * would skip this screen forever and drop straight into an engine that has
     * no game data to open.
     */
    private boolean isConfigured() {
        return GameStorage.getConfiguredRoot(this) != null;
    }

    private void requestAllFilesAccess() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                Uri.parse("package:" + getPackageName()));
        try {
            mAllFilesLauncher.launch(intent);
        } catch (android.content.ActivityNotFoundException e) {
            // Some builds only expose the global list rather than the per-app screen.
            Diag.w(C, "per-app all-files screen unavailable, falling back", e);
            try {
                mAllFilesLauncher.launch(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            } catch (android.content.ActivityNotFoundException e2) {
                Diag.e(C, "no all-files access settings screen on this device", e2);
                mStatus.setText(R.string.status_no_settings_screen);
            }
        }
    }

    private void onFolderPicked(Uri treeUri) {
        if (treeUri == null) {
            Diag.i(C, "folder picker cancelled");
            return;
        }

        // Persist the grant so the URI stays valid across restarts. This does not
        // give the engine anything -- it cannot open a content:// URI -- but it
        // keeps the choice re-resolvable without asking the user again.
        try {
            getContentResolver().takePersistableUriPermission(treeUri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (SecurityException e) {
            Diag.w(C, "could not persist URI permission", e);
        }

        File dir = GameStorage.resolveTreeUri(treeUri);
        if (dir == null) {
            Diag.w(C, "picked location has no filesystem path");
            mStatus.setText(R.string.status_folder_not_on_disk);
            return;
        }
        Diag.logDirectory(C, "picked folder", dir);

        if (!GameStorage.isUsable(dir)) {
            // The folder is real but stat() still fails, which means all-files
            // access is missing. The URI grant from the picker does not help here.
            Diag.w(C, "picked folder is not readable through POSIX: " + dir);
            mStatus.setText(getString(R.string.status_folder_unreadable, dir.getAbsolutePath()));
            return;
        }

        GameStorage.setConfiguredRoot(this, dir);
        Diag.i(C, "game root set to " + dir.getAbsolutePath());
        refresh();
    }

    private void refresh() {
        boolean access = GameStorage.hasAllFilesAccess();
        File configured = GameStorage.getConfiguredRoot(this);
        File effective = GameStorage.resolveRoot(this);
        boolean ready = GameStorage.isUsable(effective);

        GameStorage.logAccessState();

        mGrantButton.setEnabled(!access);
        mGrantButton.setText(access ? R.string.grant_access_done : R.string.grant_access);
        mChooseButton.setEnabled(access);
        mPlayButton.setEnabled(ready);
        mPlayButton.setVisibility(ready ? View.VISIBLE : View.GONE);

        if (!access) {
            mStatus.setText(R.string.status_needs_access);
        } else if (configured != null) {
            mStatus.setText(getString(R.string.status_folder_set, configured.getAbsolutePath()));
        } else {
            mStatus.setText(getString(R.string.status_choose_folder, effective.getAbsolutePath()));
        }
    }

    private void launchEngine() {
        Diag.i(C, "starting ONSActivity");
        startActivity(new Intent(this, ONSActivity.class));
        finish();
    }
}
