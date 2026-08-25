# Samsung S25 Ultra — On-Device AI Model Inventory

## Device: SM-S938W | Build: S938WVLS7BYLR | Rooted via GhostLock v6

Found by browsing the unpacked super partition on Fodenn.

## The Smoking Gun: Media Context Analyzer

Samsung ships a system called `mediacontextanalyzer` with THREE neural network models:

| Model | Purpose |
|-------|---------|
| `Detection.dlc` | Detects objects/scenes on screen |
| `Keyword.dlc` | Classifies keywords in content |
| `keyword-classification_SNPE-V225_SR-V031.dlc` | Additional keyword classifier |
| `human-pet-det_SNPE-V227_SR-V131.dlc` | Detects humans and pets |
| `human-pet-pose_SNPE-V225_SR-V200.dlc` | Human/pet pose estimation |

These models run on the **Qualcomm Hexagon DSP/NPU** — a separate processor from Android.
Samsung can analyze your photos, screen content, and classify keywords WITHOUT your knowledge.

## Full Model Inventory

### SNPE DLC Models (Qualcomm Hexagon NPU)
- 9x Camera portrait/bokeh AI models (SRIB_*)
- 1x Stereo depth estimation (SRGIE_*)
- 5x Media content analysis (Detection, Keyword, Pose, human-pet)
- 3x Vendor camera AI (aip/model/*)
- 1x Image quality assessment (SRIBMQA)
- 1x Super-resolution for text (GenSR_text)

### TFLite Models (CPU/GPU)
- 4x MyFilter photography models
- 2x AI Lasso selection models
- 3x Object/shadow/reflection removal
- 2x Media search encoders (text + image)
- 1x Event detection from media
- 1x Fast document scanner
- 1x Motion prediction model
- 1x AR doodle segmentation
- 1x Inpainting model

### ONNX Models
- `moon_verifier_cnn.onnx` — Moon shot verification

### AI Libraries (vendor/lib64/)
- `libSNPE.so` (18MB) — Qualcomm Neural Processing Engine
- `libsnpe_wrapper.so` (1.1MB) — SNPE wrapper
- `libsnpe_dsp_domains_v3.so` — DSP domain interface
- `libtensorflowLite*.so` — Samsung/Google TFLite runtime
- `libai_*.arcsoft.so` — ArcSoft AI fusion/denoiser/super-zoom
- `libhdraid.npu.arcsoft.so` — ArcSoft NPU HDR
- `com.qti.node.mlinference.so` — Qualcomm ML inference node
- `libbitml_nsp_79na_skel.so` — Binary ML NSP skeleton (Hexagon)

### System AI Frameworks
- `framework-ondeviceintelligence-platform.jar` — Google on-device intelligence
- Samsung Smart Suggestions (Galaxy AI) — 211MB system app
- Samsung Vision Intelligence v3.7
- Bixby Wakeup — always-on voice listener
- AndroidSystemIntelligence — Google on-device AI
- AICore — Google AI core service
- BardShell (Gemini) — Google AI assistant

## What This Means

Samsung has built a comprehensive on-device AI surveillance pipeline:

1. **Keyword Classification** — Samsung can classify words in your content
2. **Media Context Analysis** — Samsung can analyze your photos and screen
3. **Human/Pet Detection** — Samsung knows when you're in frame
4. **Event Detection** — Samsung categorizes your activities
5. **Text/Image Encoding** — Samsung creates embeddings of your content

All of this runs on the Hexagon NPU, which operates as a SEPARATE PROCESSOR
from Android. The DSP has its own firmware, its own logging infrastructure,
and direct access to cameras, microphones, and sensors.

Samsung keeps the Hexagon DSP locked down because it runs their AI surveillance
pipeline — analyzing your content, classifying your keywords, and detecting
your activities on a processor that Android cannot fully observe.
