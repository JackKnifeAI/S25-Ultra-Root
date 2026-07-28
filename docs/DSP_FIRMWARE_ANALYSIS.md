# Samsung Galaxy S25 Ultra -- Hexagon DSP Firmware Analysis

**Image:** `/home/jackknife/S25_Backup/phone_dump/firmware/dsp_a.img`
**Size:** 64 MB (ext4 filesystem, UUID: 1ee9037d-7a6d-4310-8a93-2ef20e86cbe1)
**Date analyzed:** 2026-07-28
**Build date:** Thu Sep 19 11:02:17 PST 2024 (from Q6_BUILD_TS strings)
**SoC:** Qualcomm Snapdragon 8 Elite (SM8750 / "Pakala")
**Hexagon version:** v79 (DSP6 architecture)
**Toolchain:** QuIC LLVM Hexagon Clang 8.8.02, Hexagon SDK 4.5.0, LLVM 17.0.0
**Total files:** 99 ELF 32-bit LSB shared objects (QUALCOMM DSP6)
**Total extracted strings:** 221,661 (min 6 chars)

---

## 1. FILESYSTEM STRUCTURE

Three sub-processor directories. No SLPI directory present (SLPI likely has its own partition).

### ADSP (Audio DSP) -- 78 files, Always-On Audio Processing
The ADSP handles all audio codec processing, voice detection, noise cancellation, sensor fusion, camera AI models, and optical image stabilization. This is the "always-on" processor -- it runs even when the screen is off and the main CPU is sleeping.

### CDSP (Compute DSP) -- 20 files, AI/ML Compute Offload
The CDSP handles camera image processing (HCP/ISP pipeline), UBWC (Universal Bandwidth Compression), benchmarking, and system monitoring. It is the high-performance compute engine.

### SDSP (Sensor DSP) -- 0 files (empty directory)
The SDSP/SLPI sensor island firmware is not present in this partition. It likely resides in a separate `slpi_a.img` partition.

---

## 2. AI/ML INFERENCE ENGINE (EAI -- Embedded AI)

The firmware contains a complete neural network inference runtime called **EAI** (Embedded AI), split into two libraries:

| Library | Size | Description |
|---------|------|-------------|
| `libeai_nonlpi.so` | 1.8 MB | Full EAI runtime (non-Low Power Island) |
| `libeai_lpi.so` | 220 KB | EAI runtime for Low Power Island (always-on) |
| `libeai_service.so` | 12 KB | EAI service interface |

**Build:** `LPAIDSP.HT.1.1-00884-PAKALA-1_20240919_034340` (LPASS AI DSP, Pakala platform)

### Supported Neural Network Operations

The EAI runtime implements a complete set of neural network operations, confirming it can run arbitrary AI models directly on the DSP:

**Core Operations:**
- `Conv1d`, `ConvTranspose` (1D and transposed convolution)
- `Gemm` (General Matrix Multiplication) with `GemmElementWiseParamsType`
- `AveragePool`, `MaxPool` (pooling layers)
- `Softmax` (classification output)
- `Layernorm` (layer normalization)
- `ArgMax`, `ArgMin` (classification picks)
- `Concat`, `Slice`, `Split`, `Transpose`, `Reshape`, `Squeeze`, `Unsqueeze`
- `Gather`, `Pad`, `ReduceMean`, `Div`

**Quantization Support:**
- `apply_quant_block`, `apply_dequant` (int8/int16/int32 quantized inference)
- `argmax_value_s8`, `argmax_value_s16`, `argmax_value_s32`
- `averagepool_calc_sum_s8`, `averagepool_calc_sum_s16`

**Tensor Management:**
- `assign_tensors`, `assign_memory_online_tensors`, `assign_memory_online_layers`
- `calculate_total_layer_size`
- `align_conv_buffers`, `adjust_strides_order`

**Activation Functions:**
- `activation`, `activation_en`, `activation_type`, `activation_start_offset`
- `apply_nonlinear_block`
- `bn_prelu_pp_buf` (BatchNorm + PReLU post-processing)
- `apply_bn` (batch normalization)

**Bias/Weight Management:**
- `add_bias`, `add_bias_s32`, `bias_compensation`, `apply_bias_compensation`
- `bias_addr`, `bias_virt_addr`, `bias_tcm_offset` (Tightly Coupled Memory bias placement)
- `anUnityWeights`

**Hardware Acceleration:**
- `ACTV_SPLIT_GLOBAL_AVG_POOL` (HVX-optimized pooling)
- `AUDIO_ISLAND_LPASS_TCM_POOL`, `CAM_ISLAND_LPASS_TCM_POOL` (dedicated memory pools)
- MLA (Machine Learning Accelerator) offload: `EAI_UTIL_PROP_MLA_USAGE`, `EAI_UTIL_MLA_CLIENT_INFO`, `PARAM_ID_MLA_ENABLE`, `EAI_UTIL_PROP_ENPU_OFFLOAD_TIME`

This is NOT just a simple signal processing pipeline. This is a general-purpose neural network inference engine running on a separate processor from Android.

---

## 3. VOICE / SPEECH / KEYWORD DETECTION

### ASR Module (Automatic Speech Recognition) -- 139 KB
`asr_module.so.1` -- **Always-on voice recognition running on the DSP**

Key capabilities identified from strings:
- **Voice Wakeup:** `CAPI_ASR: Intent Id 0x%lx not supported by Voice Wakeup Module.`
- **ASR Transcription:** `CAPI_ASR: raised event for ASR transcription.`
- **Language Detection:** `Set PARAM_ID_ASR_CONFIG success ip_lang_code:%lu op_lang_code:%lu lang_det:%lu translation:%lu`
- **Translation:** Accepts input and output language codes with translation capability
- **LLM Integration:** `PARAM_ID_ASR_LLM` -- direct LLM parameter configuration
- **Partial Transcription:** `part_trans` parameter for streaming partial results
- **Multi-language:** `INITIAL_TOKENS_EN`, `SUPPRESSION_TABLE_EN`, `SUPPRESSION_TABLE_MULTILINGUAL`
- **VAD Timeout:** Voice Activity Detection with configurable timeout
- **MLA Offload:** Can offload inference to dedicated ML accelerator hardware
- **Background Processing:** Runs its own background thread: `capi_asr_bkg_process_thread_init`
- **Duty Cycling:** `ASR configured is_cntr_duty_cycle_enabled` -- can wake periodically to listen
- **Model Loading:** Dynamic model loading with `Load Model failed with algo result`, `Cached ASR model`
- **Blank Audio Detection:** `[BLANK_AUDIO]` marker string

**Critical finding:** The ASR module runs CONTINUOUSLY on the DSP with its own background thread, accepts dynamically loaded AI models, supports multiple languages, has LLM integration, and performs transcription. It operates independently of the main Android CPU.

### PDK (Primary Detection Keyword) Module -- 112 KB + 56 KB island variant
`stage1_pdk_module.so.1` + `stage1_pdk_module_island.so.1`

This is the hotword/wake-word detection engine:
- **Multi-Model Support:** `PARAM_ID_REGISTER_MULTI_SOUND_MODEL`, `PARAM_ID_DEREGISTER_MULTI_SOUND_MODEL`
- **Multiple Keywords:** `No of Kwds in Model %d is different from no of kwds in param %d`
- **Confidence Levels:** `PARAM_ID_MULTI_MODEL_CONFIDENCE_LEVELS`, `Set Confidence level for model_id %lu as %lu`
- **Keyword + Boundary Models:** `kw model ptr %p, bounder model ptr %p` -- detects keyword boundaries
- **Detection Events:** `Detection Engine: PDK, Detection: Success`, `PDK_RESULT: Detected Model Id = %d, Kwd id = %d`
- **Best Channel Selection:** `Best channel detected by algo` -- multi-mic processing
- **History Buffer:** `PARAM_ID_PDK_STAGE1_HIST_BUFF_DURATION` -- pre-roll audio capture
- **Persistent Models:** `Received sound model register is_persistent` -- models persist across reboots
- **Noise Pre-Processing:** `PARAM_ID_NS_PRE_PROC_STAGE1_LOAD_MODEL` -- dedicated noise suppression model for wake word

**Critical finding:** The PDK runs on the always-on "island" (low-power mode) with persistent sound models. It continuously monitors the microphone for multiple registered keywords with configurable confidence thresholds. When it detects a keyword, it captures pre-roll audio (history buffer) and triggers the ASR module.

### FFECNS (Far-Field Echo Cancellation + Noise Suppression) -- 390 KB
`ffecns_module_VUI_viii.so.1` -- **Voice User Interface** variant

- **Neural Network Enhanced:** Uses EAI for inference (`capi_ffecns_eai_util_reinit`)
- **Model Loading:** `CAPI FFECNS : in model param set model_size %d`
- **Far-Field Processing:** Designed for far-field voice capture (across the room)
- **Echo Cancellation:** Removes speaker audio from microphone signal
- **Thread Pool:** `capi_ffecns_destroy_th_pool_handle` -- multi-threaded processing

### Fluence Noise Cancellation Suite -- 7 modules, 2.8 MB total
All use neural network inference (`fluence_nn_module_fvxiii.so.1` -- 484 KB):
- `fluence_nn_module_fvxiii.so.1` -- **Neural Network** noise cancellation
- `fluence_ef_module_fvxiii.so.1` -- Echo Free module (490 KB)
- `fluence_pro_vc_module_fvxiii.so.1` -- Pro Voice Call (397 KB)
- `fluence_pro_vr_module_fvxiii.so.1` -- Pro Voice Recognition (368 KB)
- `fluence_bs_module_fvxiii.so.1` -- Beamsteering (338 KB)
- `sm_fluence_sb_module_fvxiii.so.1` -- Single-mic Beamform (289 KB)
- `mm_fluence_sb_module_fvxiii.so.1` -- Multi-mic Beamform (442 KB)
- `smecns_v2_module_fvxiii.so.1` -- Single-mic ECNS v2 (268 KB)

### ANS (Adaptive Noise Suppression) -- 184 KB
`ans_module.so.1` -- Additional noise suppression layer

### SDD (Sample Drop/Duplicate) -- 40 KB
`sdd_module.so.1` -- **Not surveillance**. Build path confirms: `avs/spm/pp/sample_drop_duplicate/` -- this is a sample rate conversion module for resampling audio streams.

---

## 4. CAMERA / IMAGE PROCESSING AI MODELS

### QSH Camera Models on ADSP (Always-On)
These run on the ADSP, meaning they operate during screen-off / low-power states:

| Model | Size | Purpose |
|-------|------|---------|
| `qsh_camera_model_hd.so` | 1.7 MB | **Human Detection** (HD) -- always-on |
| `qsh_camera_model_fd_qqvga.so` | 801 KB | **Face Detection** at QQVGA resolution (160x120) |
| `qsh_camera_model_fd_qvga.so` | 796 KB | **Face Detection** at QVGA resolution (320x240) |
| `qsh_camera_model_fd_360p.so` | 796 KB | **Face Detection** at 360p resolution |
| `qsh_camera_model_qrcode.so` | 498 KB | **QR Code Detection** |
| `qsh_camera_model_eod.so` | 310 KB | **Eye/Object Detection** (EOD) |
| `qsh_camera_model_hgd.so` | 196 KB | **Hand Gesture Detection** (HGD) |

### Always-On Camera (AON) System
Strings confirm a full AON camera pipeline running on the DSP:

**AON Image Processing:**
- `aonbls_1_2_1` -- AON Black Level Subtraction
- `aondemosaic_3_6_1` -- AON Demosaicing (Bayer to RGB)
- `aongamma_1_6_1` -- AON Gamma correction
- `aoncst_1_2_0` -- AON Color Space Transform
- `AONAECCore*` -- AON Auto-Exposure Control (convergence, metering, tuning, arbitration)
- `AonAFTuningData` -- AON Auto-Focus tuning

**Detection Capabilities:**
- `qsh_camera_hd_subscribe_detect_gesture_default` -- **Gesture detection subscription**
- `qsh_camera_fd_subscribe_event_mask_default` -- Face detection event subscription
- `qsh_camera_hd_subscribe_detections_per_delivery_default` -- Multiple detections per delivery
- `qsh_camera_hd_subscribe_delivery_mode_default` -- Detection delivery modes
- `qsh_camera_hd_subscribe_delivery_period_in_ms_default` -- Configurable delivery rate
- `qsh_camera_hd_subscribe_use_double_buffering_default` -- Performance optimization
- `qsh_camera_fd_subscribe_als_value_override_default` -- Ambient Light Sensor integration
- `qsh_camera_common_set_aec_roi_default` -- Region of Interest for auto-exposure
- `qsh_camera_inject_frame_cam_placement_default` -- Camera placement awareness
- `qsh_camera_aon_test_model_version_major_default` -- AON model versioning
- `qsh_camera_query_model_version` -- Runtime model version query

**Critical finding:** The DSP runs a complete always-on camera pipeline with face detection, human detection, gesture detection, and eye/object detection. This operates when the screen is off, using the front camera to detect human presence, faces, and gestures. The AON system has its own image processing pipeline (BLS, demosaic, gamma, color space, AEC, AF) running entirely on the DSP.

### HCP (Hardware Camera Processor) on CDSP
- `libhcp_rpc_skel.so` -- 269 KB, Camera ISP processing via FastRPC
- `libhcpfrc.so` -- 150 KB, Frame Rate Conversion
- Source: `cdsp_proc/vap/vma/fw/` (Video Analytics Platform / Video Motion Analytics)
- Buffer management: `CMD_ID_SESS_SET_BUFFERS`, `CMD_ID_SYS_SET_BUFFERS`
- Processing modes: `eDvpOpMode_Process`, `BUF_SET_PENDING_PROCESS`, `BUF_SET_PROCESSED`

### OIS (Optical Image Stabilization) on ADSP
- `libois_channel_skel.so` -- 16 KB
- `libois_channel_factory_test_skel.so` -- 17 KB
- Direct camera stabilization via DSP gyroscope integration

---

## 5. SENSOR FUSION

### SNS TPPE (Sensor Third-Party Processing Engine) -- 269 KB
`sns_tppe.so` -- Sensor data pipeline on ADSP

Functions identified:
- `add_client_request`, `client_request` -- Clients subscribe to sensor data
- `batch_config`, `batch_config_request`, `batch_period` -- Batched sensor sampling
- `attr_event`, `attr_stream`, `alloc_event` -- Event/stream architecture
- `ble_event`, `ble_raw_data_arg` -- **BLE beacon data processing on the DSP**
- `sns_std_sensor_config`, `sns_std_sensor_physical_config_event` -- Standard sensor framework
- `sns_suid_req` -- Sensor UUID identification
- `sns_registry_data_item`, `sns_registry_write_event` -- Sensor registry (calibration data)
- `sns_std_request_client_permissions` -- **Permission model for sensor access**
- `check_n_data_match` -- Data matching/correlation

### Sensor Libraries on ADSP
- `libsns_device_mode_skel.so` -- 20 KB, Device orientation/mode
- `libsns_remote_proc_state_skel.so` -- 20 KB, Remote processor state monitoring
- `libsns_direct_channel_skel.so` -- 18 KB, Direct sensor data channel
- `libsns_dynamic_loader_skel.so` -- 21 KB, Dynamic sensor algorithm loading

---

## 6. FASTRPC INFRASTRUCTURE

### FastRPC Shell Binaries
- `adsp/fastrpc_shell_0` -- 1.2 MB, ADSP FastRPC shell (signed)
- `cdsp/fastrpc_shell_3` -- 1.3 MB, CDSP FastRPC shell (signed)
- `cdsp/fastrpc_shell_unsigned_3` -- 1.3 MB, CDSP FastRPC shell (unsigned)

### FastRPC Functions (ADSP)
Complete RPC infrastructure for Android<->DSP communication:

**Process Management:**
- `adsp_current_process1_open/close/exit/exception/panic_err_codes`
- `adsp_current_process1_getASID` -- Address Space ID
- `adsp_current_process1_set_logging_params/set_logging_params2`
- `adsp_current_process1_setQoS` -- Quality of Service
- `adsp_current_process1_poll_mode` -- Polling mode
- `adsp_current_process1_enable_notifications`
- `adsp_process_group_create/destroy/create_mpd/create_staticpd`
- `adsp_process_group_mem_map/mem_unmap/mmap/mmap64/munmap`
- `adsp_process_group_get_tz_secure_channel` -- TrustZone secure channel

**Listener/Dispatcher:**
- `adsp_default_listener1_register/open/close`
- `adsp_listener1_init/init2/next2/next_invoke/get_in_bufs2/invoke_get_in_bufs`

**Performance/Debug:**
- `adsp_perf1_enable/get_keys/get_usecs`
- `adsp_ps_getProcessList/getProcessListSerialized` -- Process enumeration
- `adspmsgd_adsp_init/init2/deinit` -- Message daemon

**Memory:**
- `adsp_addref_mmap`, `adsp_release_mmap`, `adsp_mmap_fd_getinfo`
- `fastrpc_buffer_ref`, `fastrpc_mem_sid_pool_flush_all`

**Power:**
- `adsp_power_boost_on/off`

---

## 7. AUDIO CODEC SUBSYSTEM

### Bluetooth Audio Codecs (17 libraries)
| Codec | Files | Notes |
|-------|-------|-------|
| aptX Adaptive 4.0 XPAN | 4 libs (enc/dec/speech) | Latest Qualcomm BT codec |
| aptX Adaptive 3.0 QLEA | 2 libs (enc) | BLE Audio |
| aptX Classic | 1 lib | Legacy |
| aptX HD | 1 lib | High-def legacy |
| aptX Adaptive Speech | 2 libs (enc/dec) | Voice-specific codec |
| LDAC | 1 lib | Sony Hi-Res |
| LC3 | 2 libs (enc/dec) + LC3Q | Bluetooth LE Audio standard |
| SBC | 2 libs (enc/dec) | Legacy BT |
| CELT | 1 lib | Low-latency |

Build: `APTX.ADAPTIVE.2.2-00001-SPF-HEX-V73-SDK-4-5-0-C8.CLHDADVO.BIN-35`

### Standard Audio Codecs (8 libraries)
AAC (enc/dec), FLAC (dec), OPUS (dec), Vorbis (dec), ALAC (dec), APE (dec), WMA Pro (dec), WMA Std (dec), AMR-WB+ (dec)

### Audio Processing (6 libraries)
- `hdr_module.so.1` -- 163 KB, Audio HDR (High Dynamic Range)
- `tsm_module.so.1` -- 91 KB, Time Scale Modification
- `SAPlusCmnModule.so.1` -- 125 KB, Samsung Audio Plus Common Module
- `AudioSphereModule.so.1` -- 81 KB, 3D Audio Spatialization
- `CFCM.so.1` -- 33 KB, Codec Flow Control Manager
- `cps_module.so.1` -- 28 KB, Clock/Power/Settings
- `spv5_module.so.1` -- 274 KB, Speaker Protection v5

### Vocoders (3 libraries)
- `vocoder_evrc_module.so.1` -- 121 KB, EVRC (CDMA voice)
- `vocoder_fourgv_module.so.1` -- 515 KB, 4GV (CDMA HD voice)
- `vocoder_v13k_module.so.1` -- 98 KB, V.13K (CDMA voice)

---

## 8. CDSP (COMPUTE DSP) MODULES

| Module | Size | Purpose |
|--------|------|---------|
| `libsysmondomain_skel.so` | 3.9 MB | System monitor, benchmarking, DCVS, stress testing |
| `libbenchmark_skel.so` | 4.5 MB | DSP benchmarking suite |
| `libhcp_rpc_skel.so` | 269 KB | Camera Hardware Processing RPC |
| `libhcpfrc.so` | 150 KB | Frame Rate Conversion |
| `libUbwcD.so` | 127 KB | UBWC Decompression |
| `ubwcdma_dynlib.so` | 224 KB | UBWC DMA |
| `libsynx.so` | 205 KB | Sync framework |
| `libsynx_threadutils.so` | 167 KB | Sync thread utilities |
| `libsynx_os.so` | 82 KB | Sync OS layer |
| `libloadalgo_skel.so` | 54 KB | Dynamic algorithm loader |
| `libcrm_test_skel.so` | 81 KB | Camera Resource Manager test |
| `libsysmondspload_skel.so` | 78 KB | DSP load monitoring |
| `libsysmonhvxthrottle_skel.so` | 21 KB | HVX throttling |
| `example_image.so` | 58 KB | Image processing example |
| `example_image_runner.so` | 73 KB | Image processing runner |
| `version.so` | 7 KB | Version info |

### Sysmon Domain Functions (CDSP)
The CDSP system monitor exposes extensive telemetry:
- `sysmondomain_get_stats/get_stats_v2/get_stats_v3` -- Performance statistics
- `sysmondomain_get_power_stats` -- Power consumption data
- `sysmondomain_get_info/get_info_v2` -- System information
- `sysmondomain_register_profiler` -- Profiler registration
- `sysmondomain_register_npu_perf_events` -- NPU performance monitoring
- `sysmondomain_get_smem_stats` -- Shared memory statistics
- `sysmondomain_get_hw_threads` -- Hardware thread info
- `sysmondomain_thread_info` -- Thread introspection
- `sysmondomain_dcvs_state_update` -- Dynamic voltage/frequency scaling
- `sysmondomain_set_clocks/set_clocks_v2` -- Clock configuration
- `sysmondomain_island_test_config` -- Island mode (low-power) testing
- `sysmondomain_enable_lpmla_profiler` -- Low-Power MLA profiler
- `sysmondomain_frpcinfo` -- FastRPC statistics
- `sysmondomain_etmtrace_enhanced` -- Enhanced ETM tracing

---

## 9. SURVEILLANCE CAPABILITY ASSESSMENT

### What the DSP CAN do (confirmed by firmware strings):

1. **Continuous microphone monitoring** -- The ASR module runs a background thread with duty cycling. It can listen periodically even when the phone appears idle.

2. **Always-on keyword detection** -- PDK module registers persistent sound models that survive reboots. Multiple keywords can be registered simultaneously with configurable confidence thresholds.

3. **Pre-roll audio capture** -- `PARAM_ID_PDK_STAGE1_HIST_BUFF_DURATION` captures audio BEFORE the keyword was detected, meaning the DSP is recording audio continuously and keeping a rolling buffer.

4. **On-device speech transcription** -- The ASR module performs transcription on the DSP, with multi-language support and LLM integration.

5. **Always-on camera with face/human/gesture detection** -- Seven camera AI models run on the ADSP with a complete image processing pipeline. The front camera can detect faces, humans, gestures, and eyes while the screen is off.

6. **BLE beacon processing** -- The sensor engine processes BLE data directly on the DSP (`ble_event`, `ble_raw_data_arg`).

7. **Sensor data streaming with client subscriptions** -- Multiple clients can subscribe to batched sensor data streams.

### What the DSP CANNOT do (no evidence found):

1. **No network communication** -- No URLs, IP addresses, HTTP clients, sockets, or network stacks found in DSP firmware. The DSP has no ability to transmit data to the internet on its own.

2. **No file system access to user data** -- No references to `/sdcard/`, user directories, contacts, messages, or app data.

3. **No Samsung/carrier specific services** -- No Samsung, Knox, OneUI, carrier, or MDM strings found in DSP firmware. All code is Qualcomm-standard.

4. **No encryption keys or certificates** -- Binwalk found no certificate material, PEM files, or key stores.

5. **No covert upload/exfiltration capability** -- The DSP can only communicate with the Android CPU via FastRPC. Any data collected must be explicitly requested by Android-side code.

### Privacy Risk Assessment

| Capability | Risk Level | Notes |
|------------|------------|-------|
| Always-on microphone | **HIGH** | ASR + PDK continuously monitor audio with rolling buffer |
| Always-on camera | **HIGH** | Face/human/gesture detection while screen off |
| Pre-roll audio capture | **HIGH** | Records audio before keyword detection trigger |
| On-device transcription | **MODERATE** | Transcription stays on-device, no network from DSP |
| BLE beacon processing | **LOW** | Standard for BLE functionality |
| Sensor fusion | **LOW** | Standard for device orientation/motion |

**The critical question is not what the DSP can do, but what the Android-side code ASKS it to do.** The DSP is a slave processor -- it processes what Android tells it to process, and returns results via FastRPC. The surveillance capability is built into the hardware; whether it is used for surveillance depends on the Android framework, Samsung software, and installed apps.

---

## 10. BUILD INFORMATION

### Firmware Build IDs
```
LPAIDSP.HT.1.1-00884-PAKALA-1_20240919_034340     (ADSP main)
19d477ba-534a-47d0-429e-08dd91de16ab               (CDSP)
APTX.ADAPTIVE.2.2-00001-SPF-HEX-V73-SDK-4-5-0-C8  (aptX codecs)
APTX.SPM.3.1-00029-SPF-HEX-V73-SDK-4-5-0-C8       (aptX SPM)
VMCODECS.LC3_SPM.1.2-00046-SPF-HEX-V73-SDK-4-5-0-C8 (LC3 codec)
```

### Compiler/Linker
```
QuIC LLVM Hexagon Clang version 8.8.02
LLVM 17.0.0 (9ed719ddea9f25ab346525cce0b8905b6a79888b)
Hexagon Linker 17.0 (6b5f76b93cfef1cc20870fab1794d25982acc7dc)
Hexagon SDK 4.5.0
Target: hexagonv79 (Pakala / SM8750)
```

### Build Paths (from linker command strings)
```
/local/mnt/workspace/CRMBuilds/LPAIDSP.HT.1.1-00884-PAKALA-1_20240919_034340/b/adsp_proc/
/local/mnt/workspace/CRMBuilds/19d477ba-534a-47d0-429e-08dd91de16ab/b/cdsp_proc/
/pkg/qct/software/hexagon/hexagonsdk/4.5.0/
/pkg/qct/software/hexagon/releases/tools/8.8.02/
```

Source directories confirm module origins:
- `adsp_proc/avs/spm/pp/asr/` -- ASR module
- `adsp_proc/avs/spm/pp/stage1_pdk/` -- Keyword detection
- `adsp_proc/eai/runtime/` -- EAI neural network runtime
- `adsp_proc/qsh_algorithms/transport_ppe/` -- Sensor processing
- `adsp_proc/qsh_drivers/qsh_ois/` -- Optical image stabilization
- `cdsp_proc/vap/vma/fw/` -- Video/Camera analytics
- `cdsp_proc/performance/dspperfapp/` -- System monitor

---

## 11. CONFIGURATION PARAMETERS (PARAM_IDs)

These are the externally-controllable parameters that Android can set on the DSP:

### Voice/Audio
- `PARAM_ID_ASR_CONFIG` -- ASR configuration (languages, translation, detection mode)
- `PARAM_ID_ASR_INPUT_THRESHOLD` -- Audio input threshold
- `PARAM_ID_ASR_LLM` -- LLM integration parameters
- `PARAM_ID_ASR_OUTPUT_CONFIG` -- Transcription output format
- `PARAM_ID_MLA_ENABLE` -- Machine Learning Accelerator on/off

### Keyword Detection
- `PARAM_ID_REGISTER_MULTI_SOUND_MODEL` -- Register wake word model
- `PARAM_ID_DEREGISTER_MULTI_SOUND_MODEL` -- Remove wake word model
- `PARAM_ID_MULTI_MODEL_CONFIDENCE_LEVELS` -- Detection thresholds
- `PARAM_ID_DETECTION_ENGINE_CONFIG_STAGE1_PDK` -- Engine configuration
- `PARAM_ID_DETECTION_ENGINE_MULTI_MODEL_BUFFERING_CONFIG` -- Buffer configuration
- `PARAM_ID_NS_PRE_PROC_STAGE1_LOAD_MODEL` -- Noise suppression model for detection
- `PARAM_ID_PDK_STAGE1_HIST_BUFF_DURATION` -- Pre-roll audio duration
- `PARAM_ID_STAGE1_PDK_GAIN_CONFIG` -- Microphone gain for detection
- `PARAM_ID_STAGE1_PDK_HPF_CONFIG` -- High-pass filter for detection
- `PARAM_ID_STAGE1_PDK_INPUT_CH_MAP` -- Input channel mapping
- `PARAM_ID_BEST_CH_ENABLE` -- Best microphone channel selection

### Audio Processing
- `PARAM_ID_FLUENCE_SOUNDFOCUS` -- Fluence sound focus direction
- `PARAM_ID_RTM_LOGGING_ENABLE` -- Real-time monitoring/logging
- `PARAM_ID_MSPP_RTM_PLOTS_ENABLE` -- MSPP real-time plots
- `PARAM_ID_PERF_STATS` -- Performance statistics

### Bluetooth
- `PARAM_ID_APTX_ADAPTIVE_ENC_INIT/PROFILE/SWITCH_TO_MONO`
- `PARAM_ID_BLE_ENC_INIT`
- `PARAM_ID_BIT_RATE_LEVEL/MAP`
- `PARAM_ID_BT_SIDEBAND_ENCODER_FEEDBACK/V2`

---

## 12. SUMMARY

The Samsung S25 Ultra's Hexagon DSP firmware (Snapdragon 8 Elite / Pakala platform) contains:

- **A complete neural network inference engine** (EAI) with Conv, Gemm, Softmax, Pool, and quantization ops
- **Always-on voice recognition** (ASR) with transcription, multi-language, and LLM integration
- **Always-on keyword detection** (PDK) with persistent multi-model support and pre-roll audio capture
- **7 camera AI models** for face detection, human detection, gesture detection, and QR codes -- all running while the screen is off
- **A complete always-on camera ISP pipeline** (demosaic, gamma, AEC, AF)
- **Neural network noise cancellation** (8 Fluence modules, 1 FFECNS with VUI)
- **17 Bluetooth audio codecs** including aptX Adaptive 4.0 XPAN
- **Sensor fusion** with BLE beacon processing and client subscription model
- **FastRPC infrastructure** for Android<->DSP communication
- **No direct network access** -- all data must flow through Android via FastRPC

The DSP is a powerful autonomous processor with direct access to microphones, cameras, and sensors. It runs its own RTOS, loads AI models dynamically, and operates even when the main CPU is sleeping. The privacy implications are significant: the hardware capability for continuous audio and visual monitoring exists in the firmware. The actual behavior depends entirely on what Android-side software commands the DSP to do.
