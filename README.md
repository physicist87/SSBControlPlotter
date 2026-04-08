# ControlPlots — ttbar CPV Analysis Histogram Stacking Framework

> CMS Run 2 UL ttbar dilepton CP violation analysis  
> Histogram stacking, DY scale factor application, channel/Run2 combination

---

## Table of Contents / 목차

1. [Overview / 개요](#overview)
2. [File Structure / 파일 구조](#file-structure)
3. [Configuration Files / 설정 파일](#configuration-files)
4. [Usage / 사용법](#usage)
5. [Step-wise DY Scale Factor / 단계별 DY Scale Factor](#step-wise-dy-scale-factor)
6. [Category Mode / 카테고리 모드](#category-mode)
7. [Channel & Run2 Combination / 채널 및 Run2 합산](#channel--run2-combination)
8. [CMS Color Palette / CMS 공식 색상](#cms-color-palette)

---

## Overview

**EN:** This framework reads per-sample ROOT histogram files, applies step-wise DY scale factors derived from data-driven DY estimation, stacks histograms by physics process category, and produces CMS-style plots with Data/MC ratio panels. It supports single-channel, Dilepton (combined channel), per-RunPeriod, and full Run2 combined outputs.

**KR:** 이 프레임워크는 샘플별 ROOT 히스토그램 파일을 읽어 데이터 기반 DY estimation에서 도출된 단계별 DY scale factor를 적용하고, 물리 프로세스 카테고리별로 히스토그램을 스택하여 Data/MC ratio 패널을 포함한 CMS 스타일 플롯을 생성합니다. 단일 채널, Dilepton(채널 합산), RunPeriod별, 전체 Run2 합산 출력을 지원합니다.

---

## File Structure

```
ControlPlots/
├── StackAndOverlayHistograms.cpp   # Main analysis executable
├── Makefile                        # Build configuration
├── run_analysis.py                 # Job runner script
├── Generate_ScaleConfig_From_DYSF.py  # ScaleConfig generator from DYSF file
│
├── ColorConfig.txt                 # Color assignments per category
├── HistConfig.txt                  # Rebin & x-axis label per histogram
├── ScaleConfig.txt                 # Default scale config (fallback)
│
├── DYSF_Run2UL.txt                 # DY scale factors (input for generator)
│
├── ScaleConfig/                    # Per-RunPeriod/Channel ScaleConfig files
│   └── AN_v6p4/
│       └── {RunPeriod}/{Channel}/
│           └── ScaleConfig_{STUDYNAME}_{RunPeriod}_{Channel}.txt
│
├── input/                          # Input ROOT file lists
│   └── AN_v6p4/
│       └── {RunPeriod}/{Channel}/
│           └── AN_v6p4_{RunPeriod}_{Channel}.list
│
└── Histograms/                     # Output plots (auto-created)
    └── AN_v6p4_DYEstApplied_Category/
        └── {RunPeriod}/{Channel}/
            └── {histName}_Log.pdf / _Log.png / _Linear.pdf / _Linear.png
```

---

## Configuration Files

### `ColorConfig.txt`

**EN:** Assigns a color to each physics process category. Supports both CMS official color indices (`10001`–`10010`) and legacy ROOT color names (`kRed +0`).

**KR:** 각 물리 프로세스 카테고리에 색상을 지정합니다. CMS 공식 색상 인덱스(`10001`–`10010`)와 기존 ROOT 색상 이름(`kRed +0`) 형식을 모두 지원합니다.

```
# Category      ColorIndex
Data            kBlack
TTbar_Signal    10003
TTbar_Others    10007
SingleTop       10005
DY              10001
Diboson         10002
WJets           10004
TTV             10010
```

---

### `HistConfig.txt`

**EN:** Defines rebin factor and x-axis label for each histogram pattern.

**KR:** 각 히스토그램 패턴에 대한 rebin factor와 x축 레이블을 정의합니다.

```
# HistPattern    RebinFactor   XAxisLabel
h_DiLepMass      5             Inv.Mass[GeV]
h_Jet1pt_        10            Leading\\Jet\\p_{T}[GeV]
```

---

### `ScaleConfig/{STUDYNAME}/{RunPeriod}/{Channel}/ScaleConfig_*.txt`

**EN:** Per-channel, per-RunPeriod scale factors. Non-DY samples use a flat SF of `1.0`. DY uses step-wise SFs matched to the selection stage encoded in the histogram name suffix (`_0`–`_9`).

**KR:** 채널별, RunPeriod별 scale factor 파일입니다. Non-DY 샘플은 `1.0`의 고정 SF를 사용하고, DY는 히스토그램 이름 suffix(`_0`–`_9`)에 인코딩된 셀렉션 단계에 맞는 단계별 SF를 사용합니다.

```
# Non-DY samples
TTbar_Signal    1.0
...

# DY step-wise SF
# step 0    -> 1.0 (no selection)
# step 1,2  -> Dilepton selection
# step 3,4  -> >= 2 jets selection
# step 5~9  -> >= 1 b-tag selection
# step *    -> >= 1 b-tag (suffix-less histograms: h_Toppt, h_Reco_CPO*, ...)
DY  0    1.000000
DY  1    0.994256
DY  2    0.994256
DY  3    0.997269
...
DY  *    1.230428
```

---

## Usage

### Step 1 — Build / 빌드

```bash
make -f Makefile
```

### Step 2 — Generate ScaleConfig files / ScaleConfig 파일 생성

**EN:** Run once to generate all per-RunPeriod/Channel ScaleConfig files from `DYSF_Run2UL.txt`.

**KR:** `DYSF_Run2UL.txt`로부터 RunPeriod/Channel별 ScaleConfig 파일을 생성합니다. 처음 한 번만 실행하면 됩니다.

```bash
python3 Generate_ScaleConfig_From_DYSF.py
```

Output: `ScaleConfig/AN_v6p4/{RunPeriod}/{Channel}/ScaleConfig_*.txt` (12 files total)

### Step 3 — Run analysis / 분석 실행

```bash
python3 run_analysis.py
```

**EN:** Runs all job combinations configured in `run_analysis.py`. Output plots are saved under `Histograms/`.

**KR:** `run_analysis.py`에 설정된 모든 job 조합을 실행합니다. 출력 플롯은 `Histograms/` 아래에 저장됩니다.

### Manual execution / 직접 실행

```bash
./StackAndOverlayHistograms \
    <input_list>    \   # comma-separated for multi-channel
    ColorConfig.txt \
    <scale_config>  \   # comma-separated for multi-channel
    HistConfig.txt  \
    <output_dir>    \
    "lumi text"     \
    <category_mode> \   # 1 = category, 0 = individual
    <channel>           # MuMu / ElEl / MuEl / Dilepton (optional)
```

---

## Step-wise DY Scale Factor

**EN:** The DY background scale factor varies by event selection stage. The stage is encoded in the histogram name suffix:

**KR:** DY 배경 scale factor는 이벤트 셀렉션 단계에 따라 달라집니다. 단계는 히스토그램 이름 suffix에 인코딩되어 있습니다:

| Suffix | Selection Stage (EN) | 셀렉션 단계 (KR) | DYSF Key |
|--------|---------------------|-----------------|----------|
| `_0`   | No selection        | 셀렉션 없음      | 1.0 (fixed) |
| `_1`, `_2` | Dilepton selection | 딜렙톤 셀렉션 | Dilepton |
| `_3`, `_4` | ≥ 2 jets          | 제트 ≥ 2       | ≥ 2 jets |
| `_5` ~ `_9` | ≥ 1 b-tag        | b-태그 ≥ 1     | ≥ 1 b-tag |
| none   | Suffix-less (h_Toppt, h_Reco_CPO*, ...) | Suffix 없음 | ≥ 1 b-tag |

**EN:** To regenerate ScaleConfig files after updating `DYSF_Run2UL.txt`, simply re-run `Generate_ScaleConfig_From_DYSF.py`.

**KR:** `DYSF_Run2UL.txt`를 업데이트한 후 ScaleConfig 파일을 재생성하려면 `Generate_ScaleConfig_From_DYSF.py`를 다시 실행하면 됩니다.

---

## Category Mode

**EN:** Controlled by `USE_CATEGORY_MODE` in `run_analysis.py`.

**KR:** `run_analysis.py`의 `USE_CATEGORY_MODE`로 제어합니다.

```python
USE_CATEGORY_MODE = True   # Merge samples into physics categories
USE_CATEGORY_MODE = False  # Show individual MC samples
```

| Category | Samples included |
|----------|-----------------|
| `TTbar_Signal` | TTbar_Signal |
| `TTbar_Others` | TTbar_AllHadronic, TTbar_SemiLeptonic |
| `DY` | DYJetsToLL_M_50, DYJetsToLL_M_10To50, ... |
| `SingleTop` | ST_t-channel, ST_tW, ST_s-channel, ... |
| `WJets` | WJetsToLNu |
| `Diboson` | WW, WZ, ZZ |
| `TTV` | TTZToQQ, TTZToLLNuNu, TTWJetsToLNu, ... |

---

## Channel & Run2 Combination

**EN:** `run_analysis.py` runs four job types, each controllable with a flag:

**KR:** `run_analysis.py`는 네 가지 job 유형을 실행하며, 각각 플래그로 제어합니다:

```python
RUN_CHANNEL_COMBINE = True   # [2] Dilepton: MuMu + ElEl + MuEl per RunPeriod
RUN_RUN2_COMBINE    = True   # [3] Run2: all RunPeriods per channel
RUN_RUN2_DILEPTON   = True   # [4] Run2 + Dilepton: everything combined
```

| Job | Output directory | Description |
|-----|-----------------|-------------|
| [1] | `{STUDYNAME}_DYEstApplied_Category/{RunPeriod}/{Channel}/` | Per RunPeriod, per channel |
| [2] | `{STUDYNAME}_DYEstApplied_Category/{RunPeriod}/Dilepton/` | Channel combined |
| [3] | `{STUDYNAME}_DYEstApplied_Category/Run2/{Channel}/` | Run2 combined |
| [4] | `{STUDYNAME}_DYEstApplied_Category/Run2/Dilepton/` | Full Run2 + Dilepton |

**EN:** For multi-channel jobs, input lists and ScaleConfig files are passed as comma-separated strings. Each channel's DY SF is applied independently before histograms are summed.

**KR:** 멀티채널 job의 경우 input list와 ScaleConfig 파일이 콤마로 구분된 문자열로 전달됩니다. 각 채널의 DY SF는 히스토그램을 합산하기 전에 독립적으로 적용됩니다.

---

## CMS Color Palette

**EN:** CMS official colors are registered automatically at runtime via `registerCMSColors()`.

**KR:** CMS 공식 색상은 `registerCMSColors()`를 통해 런타임에 자동으로 등록됩니다.

| Index | RGB | Color | Assigned to |
|-------|-----|-------|-------------|
| 10001 | (63, 144, 218) | Azure Blue | DY |
| 10002 | (255, 169, 14) | Orange Yellow | Diboson |
| 10003 | (189, 31, 1) | Deep Red | TTbar_Signal |
| 10004 | (148, 164, 162) | Grey Green | WJets |
| 10005 | (131, 45, 182) | Purple | SingleTop |
| 10006 | (169, 107, 89) | Brown | — |
| 10007 | (231, 99, 0) | Deep Orange | TTbar_Others |
| 10008 | (185, 172, 112) | Olive | — |
| 10009 | (113, 117, 129) | Grey | — |
| 10010 | (146, 218, 221) | Teal | TTV |

**EN:** Alternatively, register colors globally via `~/.rootlogon.C` so they are available in all ROOT sessions.

**KR:** 또는 `~/.rootlogon.C`에 등록하면 모든 ROOT 세션에서 사용할 수 있습니다.

```cpp
// ~/.rootlogon.C
void _rootlogon() {
    new TColor(10001,  63./255., 144./255., 218./255.);
    new TColor(10002, 255./255., 169./255.,  14./255.);
    new TColor(10003, 189./255.,  31./255.,   1./255.);
    new TColor(10004, 148./255., 164./255., 162./255.);
    new TColor(10005, 131./255.,  45./255., 182./255.);
    new TColor(10006, 169./255., 107./255.,  89./255.);
    new TColor(10007, 231./255.,  99./255.,   0./255.);
    new TColor(10008, 185./255., 172./255., 112./255.);
    new TColor(10009, 113./255., 117./255., 129./255.);
    new TColor(10010, 146./255., 218./255., 221./255.);
}
```
