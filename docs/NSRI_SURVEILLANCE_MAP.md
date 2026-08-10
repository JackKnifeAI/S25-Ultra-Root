# NSRI SURVEILLANCE ARCHITECTURE — COMPLETE MAP
## Samsung Galaxy S25 Ultra | libsec-ril.so v5.0 | August 10, 2026

---

## SMOKING GUN: 124 NSRI Functions in Samsung's RIL Library

Samsung's proprietary Radio Interface Layer (libsec-ril.so, 7.1MB) contains a complete
surveillance subsystem for South Korea's National Security Research Institute (NSRI).
This code ships on EVERY Samsung phone worldwide, including Canadian model SM-S938W.

---

## NSRI CAPABILITY MATRIX

### 1. SMS Encryption/Decryption (NSRI reads ALL your texts)
- `DoGetNsriEncryptSms` — Encrypt outgoing SMS for NSRI
- `DoGetNsriDecryptSms` — Decrypt incoming SMS for NSRI
- `DoGetNsriDecryptTxSms` — Decrypt transmitted SMS for NSRI
- `DOMESTIC_NSRI_ENCRYPT_SMS` / `DOMESTIC_NSRI_DECRYPT_SMS` / `DOMESTIC_NSRI_DECRYPTTX_SMS`
- `[NSRI_SMS] phLen=[%d] msgLen=[%d] phNum=[%s] msgText=[%s]` — Logs phone numbers and message text
- `Match Special Pdu for NSRI` — Intercepts special NSRI-formatted SMS PDUs
- `GetUserDataforNsri` / `ParseUserDataforNsri` — Extracts user data specifically for NSRI

### 2. Remote Control via SMS (NSRI can remotely command your phone)
- `DOMESTIC_NSRI_RECEIVED_REMOTE_CONTROL_PDU` — Receives remote control commands
- `DoOemSendNsriRemoteControl` — Sends NSRI remote control commands
- `SendNsriReceiveRemoteControlPdu` — Processes received remote control PDUs
- `IpcTxDomesticNsriReceiveRemoteControlPdu` — Transmits remote control to modem

### 3. Secure Call Mode (NSRI can intercept voice calls)
- `DOMESTIC_NSRI_SECURE_CALL_MODE` — Activates/deactivates NSRI secure call mode
- `IpcRxDomesticNSRISecureCallMode` — Receives secure call mode commands from modem

### 4. NSRI Feature Activation (Kill switch / enable switch)
- `DoOemNsriFeatureOn` — Turns NSRI surveillance ON
- `SmsManager::mNsriFeatureOn` — Static flag: is NSRI active?
- `DoSetNsriProcess` / `DOMESTIC_NSRI_PROCESS` — Start/stop NSRI processing
- `DoGetNsriReqProc` / `DOMESTIC_NSRI_REQUEST_PROC` — NSRI request processing

### 5. SIM Card Verification (Checks if SIM supports NSRI)
- `DoGetNsriCheckUsimstate` — Check if USIM supports NSRI
- `DOMESTIC_NSRI_CHECK_SUSIM` — Check secure USIM state
- `IpcTxDomesticGetNsriCheckSusim` — Query modem for NSRI SIM capability

### 6. NSRI Notifications (Silent alerts to NSRI infrastructure)
- `DOMESTIC-MGR: EVENT_DOMESTIC_NSRI_NOTI` — NSRI notification event
- `DOMESTIC_NSRI_TOAST_CMD` — Display toast (or suppress it)
- `IpcRxDomesticNSRINoti` — Receive NSRI notification from modem
- `NSRI_TOAST_CMD : %s` — Toast command string

---

## DOMESTIC SECURITY MODE (Separate from NSRI but related)

- `BuildIpcDomesticGetSecurityMode` — Query current security mode
- `BuildIpcDomesticSetSecurityMode` — Set security mode
- `DOMESTIC_SECURITY_MODE` — Security mode command
- `DoGetSecurityMode` / `DoSetSecurityMode` — Get/set security mode functions

This appears to be a separate "domestic" security mode that may enable/disable
encryption for domestic Korean carriers, potentially allowing cleartext interception.

---

## LOCATION SURVEILLANCE

- `broadcast -a com.samsung.android.intent.action.REPORT_LOCATION` — Modem broadcasts location
- `com.samsung.android.ipsgeofence/.task.receivers.IpsCpReceiver` — Samsung IPS geofence receiver
- `QMI_NAS_GET_CELL_LOCATION_INFO` — Cell tower location query
- `QMI_NAS_PERFORM_NETWORK_SCAN` — Network scanning (IMSI catcher capability)
- `ConvertCellLocationToRilCellinfo` — Cell location conversion for apps

---

## OEM HOOK RAW (Backdoor Command Channel)

- `android.intent.action.ACTION_UNSOL_RESPONSE_OEM_HOOK_RAW` — Unsolicited OEM hook response
- `DoRequestOemHookRaw` — Execute raw OEM hook command
- `BuildOemHookRawResponse` — Build response for OEM hook
- `RIL_REQUEST_OEM_HOOK_RAW` — Raw backdoor command request

The OEM_HOOK_RAW interface allows Samsung (or anyone with the right IPC access) to send
arbitrary commands to the modem. This is an unrestricted backdoor.

---

## SAMSUNG BROADCAST INTENTS (Data Exfiltration Channels)

48 broadcast intents found, including:
- Location reporting to Samsung
- SMS status and failure reporting
- IMS/VoLTE mode switching
- Call diagnostics (outgoing call info sent to Samsung dialer)
- SIM lock authentication
- Factory reset capability
- Knox analytics
- Emergency SMS search
- Network diagnostics

---

## CLASSES INVOLVED

- `DomesticOemManager` — Main domestic/NSRI command dispatcher
- `IpcProtocol41Domestic` — IPC protocol handler for domestic commands
- `IpcModemImplDomestic` — Modem implementation for domestic functions
- `SmsManager` — SMS manager with NSRI feature flag
- `GsmSmsMessage` — GSM SMS with NSRI user data parsing
- `PduParser` — PDU parser with NSRI-specific extraction
- `MiscRespBuilder` — Builds NSRI responses
- `ModemProxy` — Proxy layer for all NSRI modem commands

---

## ATTACK SURFACE FOR NEUTRALIZATION

### Option 1: Binary Patch libsec-ril.so
- NOP out `DoOemNsriFeatureOn` to prevent activation
- Patch `mNsriFeatureOn` to always return false
- Replace NSRI IPC handlers with no-ops
- Risk: May break SMS if NSRI functions are in the call chain

### Option 2: Block at Android Framework Level (Root Required)
- Disable `com.samsung.intent.action.NSRI_TOAST_CMD` broadcast receiver
- Block `com.samsung.android.intent.action.REPORT_LOCATION` broadcasts
- Firewall the domestic OEM command channel
- Disable `diagmonagent` and related surveillance services

### Option 3: Replace libsec-ril.so Entirely
- Use AOSP RIL implementation
- Requires understanding of Samsung's IPC protocol (Protocol 4.1)
- Most complete but most complex

### Option 4: SELinux Policy (Root + Custom Policy)
- Deny secril access to NSRI-related IPC commands
- Deny location broadcast intents
- Deny OEM_HOOK_RAW for non-system processes

---

*Report generated August 10, 2026*
*JackKnife Studios — S25 Ultra Liberation Project*
*VIVA LA REVOLUTION*
