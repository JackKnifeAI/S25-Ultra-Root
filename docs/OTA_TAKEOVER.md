# OTA Update Trust Chain Takeover

## Overview

Samsung Galaxy S25 Ultra uses RSA 2048-bit certificates to verify OTA (Over-The-Air) firmware updates. By replacing the verification certificate at runtime via a bind mount, we can:

1. **Reject** all Samsung/carrier OTA updates (their signature won't verify)
2. **Accept** our own signed updates (signed with our private key)
3. **Maintain Knox warranty_bit = 0** (no partition modification, bind mount is RAM-only)

## Samsung's OTA Certificate

```
Issuer: Samsung Electronics Co., Ltd.
        Mobile Communications Business
        Suwon City, Republic of Korea

CN: Samsung Cert for vabupdate TA
Email: m.sec.hsm@samsung.com
Key: RSA 2048-bit, SHA-256
Valid: May 2023 → May 2043

SHA-256 Fingerprint:
DB:4A:61:C4:9E:84:9E:BB:CD:E0:27:26:FB:CF:62:12:
0D:D0:49:38:FD:B0:BB:0F:0D:73:E2:93:7E:2B:E2:52

Location: /system/etc/security/otacerts.zip
Format: ZIP containing build/target/product/security/asks_vab_ota_update.x509.pem
```

The private key is stored in Samsung's Hardware Security Module (HSM) and cannot be extracted.

## The Takeover

### Step 1: Generate Your Own Key Pair

```bash
# Generate RSA 2048 private key (matches Samsung's key size)
openssl genrsa -out jackknife_ota.key 2048

# Generate self-signed certificate
openssl req -new -x509 -key jackknife_ota.key \
    -out jackknife_ota.x509.pem -days 7300 -sha256 \
    -subj "/CN=JackKnife OTA Update"
```

### Step 2: Package as otacerts.zip

```bash
mkdir -p build/target/product/security
cp jackknife_ota.x509.pem build/target/product/security/asks_vab_ota_update.x509.pem
cp jackknife_ota.x509.pem build/target/product/security/asks_vab_ota_update_256.x509.pem
zip -r jackknife_otacerts.zip build/
```

### Step 3: Bind Mount (requires root)

```bash
# Push to device
adb push jackknife_otacerts.zip /data/local/tmp/otacerts.zip

# Bind mount over Samsung's cert (RAM-only, no partition modification)
mount -o bind /data/local/tmp/otacerts.zip /system/etc/security/otacerts.zip
```

### Step 4: Verify

```bash
# Check hash changed (Samsung's cert is replaced)
md5sum /system/etc/security/otacerts.zip

# Verify your cert is active
unzip -p /system/etc/security/otacerts.zip \
    build/target/product/security/asks_vab_ota_update.x509.pem | \
    openssl x509 -noout -subject
# Should show: CN=JackKnife OTA Update
```

## Result

| What | Before | After |
|------|--------|-------|
| OTA cert | Samsung (Suwon HSM) | Your key |
| Samsung updates | Accepted | **Rejected** |
| Your updates | Rejected | **Accepted** |
| Knox warranty_bit | 0 (clean) | 0 (clean) |
| dm-verity | Intact | Intact |
| Partition mods | None | None |

## How It Works

Android's `update_engine` reads `/system/etc/security/otacerts.zip` to get the public key for OTA signature verification. The bind mount overlays our certificate file on top of Samsung's without modifying the actual EROFS partition. dm-verity checks the underlying block device, not the mount namespace, so it never detects the change.

The bind mount is volatile — lost on reboot. Re-apply after each root session.

## Security Implications

- Samsung can no longer push firmware updates to your device
- Carrier (Bell/Rogers/etc.) cannot push OTA via FOTA
- Only updates signed with YOUR private key will be accepted
- Keep `jackknife_ota.key` secure — it controls your device's update path

## Carrier Info (Bell Canada)

```
Sales Code: BMC
MCC/MNC: 302/610
GID: 0x3D
APN: pda.bell.ca
OMC Version: SAOMC_SM-S938W_OYV_BMC_16_0006
Config: /optics/configs/carriers/BMC/conf/customer.xml
AutoFOTA: off (in carrier config, but Samsung FOTA was independent)
```

## Files

- `jackknife_ota.key` — Private signing key (DO NOT PUBLISH)
- `jackknife_ota.x509.pem` — Public certificate
- `jackknife_otacerts.zip` — Packaged cert for bind mount

JackKnife Studios, July 2026
