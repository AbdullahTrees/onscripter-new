package org.umineko_project.onscripter_ru;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.button.MaterialButton;

/**
 * Shows the last crash report.
 *
 * Reached two ways. A Java exception starts this activity from Diag's uncaught
 * handler, before the process goes. A native crash cannot do that -- the engine
 * takes the process down with it -- so SetupActivity checks on the next launch
 * whether the previous process died abnormally and starts this then.
 */
public class CrashActivity extends AppCompatActivity {
    private static final String C = "CrashActivity";

    private String report = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_crash);

        report = CrashReport.read(this);
        ((TextView) findViewById(R.id.crash_log)).setText(report);

        // The <a href> in crash_body already arrives as a URLSpan, so the link
        // looks like a link with no help from here. Without a MovementMethod
        // nothing dispatches the touch to it, and tapping does nothing at all.
        ((TextView) findViewById(R.id.crash_body))
                .setMovementMethod(LinkMovementMethod.getInstance());

        MaterialButton share = findViewById(R.id.share_logs);
        MaterialButton copy = findViewById(R.id.copy_logs);
        MaterialButton restart = findViewById(R.id.restart);

        share.setOnClickListener(v -> shareReport());
        copy.setOnClickListener(v -> copyReport());
        restart.setOnClickListener(v -> restart());
    }

    private void shareReport() {
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_SUBJECT, getString(R.string.crash_share_subject));
        send.putExtra(Intent.EXTRA_TEXT, report);
        try {
            startActivity(Intent.createChooser(send, getString(R.string.crash_share)));
        } catch (RuntimeException e) {
            // A very large report can exceed the binder limit on some devices.
            Diag.w(C, "sharing failed, falling back to clipboard", e);
            copyReport();
        }
    }

    private void copyReport() {
        ClipboardManager clipboard = getSystemService(ClipboardManager.class);
        if (clipboard == null) {
            return;
        }
        clipboard.setPrimaryClip(ClipData.newPlainText("crash log", report));
        Toast.makeText(this, R.string.crash_copied, Toast.LENGTH_SHORT).show();
    }

    /**
     * Starts a clean run. The engine cannot be resumed in this process -- after
     * a native fault its state is undefined, and SDLActivity refuses a second
     * onCreate anyway -- so this goes back through the launcher screen.
     */
    private void restart() {
        Intent intent = new Intent(this, SetupActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(intent);
        finish();
    }

    /** Builds the intent that opens this screen. */
    static Intent intentFor(Context context) {
        Intent intent = new Intent(context, CrashActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        return intent;
    }
}
