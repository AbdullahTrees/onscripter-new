package org.umineko_project.onscripter_ru;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.appcompat.app.AppCompatActivity;

import java.util.List;

/**
 * Relaunches the native engine after the old default process has exited.
 *
 * This activity runs in its own process. ONSActivity cannot be initialized a
 * second time in one process, while its singleTask launch mode would otherwise
 * route a folder change back to the existing engine through onNewIntent().
 */
public final class RestartActivity extends AppCompatActivity {
    private static final String C = "RestartActivity";
    private static final String EXTRA_OLD_PID = "old_pid";
    private static final long CHECK_INTERVAL_MS = 25;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private int oldPid;

    private final Runnable awaitOldProcess = new Runnable() {
        @Override
        public void run() {
            if (!isProcessRunning(oldPid)) {
                launchEngine();
                return;
            }
            handler.postDelayed(this, CHECK_INTERVAL_MS);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_restart);

        oldPid = getIntent().getIntExtra(EXTRA_OLD_PID, -1);
        Diag.i(C, "waiting for engine process " + oldPid + " to exit");
        handler.post(awaitOldProcess);
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacks(awaitOldProcess);
        super.onDestroy();
    }

    private boolean isProcessRunning(int pid) {
        if (pid <= 0 || pid == android.os.Process.myPid()) {
            return false;
        }
        ActivityManager manager = getSystemService(ActivityManager.class);
        List<ActivityManager.RunningAppProcessInfo> processes =
                manager == null ? null : manager.getRunningAppProcesses();
        if (processes == null) {
            // The old process was synchronously sent SIGKILL before this
            // coordinator could start. A missing process list is therefore a
            // completed shutdown, not a reason to reuse the stale activity.
            return false;
        }
        for (ActivityManager.RunningAppProcessInfo process : processes) {
            if (process.pid == pid) {
                return true;
            }
        }
        return false;
    }

    private void launchEngine() {
        Diag.i(C, "old process exited; launching a clean engine process");
        Intent intent = new Intent(this, ONSActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(intent);
        finish();
    }

    static Intent intentFor(Context context, int oldPid) {
        Intent intent = new Intent(context, RestartActivity.class);
        intent.putExtra(EXTRA_OLD_PID, oldPid);
        return intent;
    }
}
