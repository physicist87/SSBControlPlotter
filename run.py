#!/usr/bin/env python3

import subprocess
import os

# Define the executable
EXEC = "./StackAndOverlayHistograms"

# Study name
#STUDYNAME = "AN_v6p2-12_v3"
#STUDYNAME = "AN_v6p2-12"
STUDYNAME = "AN_v6p2-13"
STUDYNAME = "AN_v6p4"
#STUDYNAME = "MakePseudo_v1_v3"

# Run periods and channels to loop over
#RUNPERIODS = ["UL2016PreVFP", "UL2016PostVFP", "UL2017", "UL2018"]
#RUNPERIODS = ["UL2017"]
#RUNPERIODS = ["UL2016PostVFP"]
#RUNPERIODS = ["UL2016PreVFP"]
#RUNPERIODS = ["UL2018"]
RUNPERIODS = ["UL2016PreVFP", "UL2016PostVFP", "UL2017", "UL2018"]
#CHANNELS = ["MuMu"]
CHANNELS = ["MuMu", "MuEl", "ElEl"]

# Category mode: True = merged categories, False = individual samples
#USE_CATEGORY_MODE = False
USE_CATEGORY_MODE = True

# === Combine options ===
# RUN_CHANNEL_COMBINE: also run a Dilepton (MuMu+ElEl+MuEl) combined output per RunPeriod
# RUN_RUN2_COMBINE:    also run a Run2 combined output per channel (all RunPeriods)
# RUN_RUN2_DILEPTON:   also run the full Run2 + Dilepton combined output
RUN_CHANNEL_COMBINE = True
RUN_RUN2_COMBINE    = True
RUN_RUN2_DILEPTON   = True

# Luminosity text mapping
LUMI_TEXTS = {
    "UL2018":        "UL 2018 59.83 fb^{-1} (13 TeV)",
    "UL2017":        "UL 2017 41.48 fb^{-1} (13 TeV)",
    "UL2016PostVFP": "UL 2016NonAPV 16.67 fb^{-1} (13 TeV)",
    "UL2016PreVFP":  "UL 2016APV 19.65 fb^{-1} (13 TeV)",
    "Run2":          "Run2 UL 138.0 fb^{-1} (13 TeV)",
}

# Configuration files (static)
COLOR_CONFIG = "ColorConfig.txt"
HIST_CONFIG  = "HistConfig.txt"

mode_suffix = "_Category" if USE_CATEGORY_MODE else "_Individual"


def get_scale_config(runperiod, channel):
    return f"ScaleConfig/{STUDYNAME}/{runperiod}/{channel}/ScaleConfig_{STUDYNAME}_{runperiod}_{channel}.txt"


def get_input_list(runperiod, channel):
    return f"input/{STUDYNAME}/{runperiod}/{channel}/{STUDYNAME}_{runperiod}_{channel}.list"


def run_job(input_lists, scale_configs, output_dir, lumi_text, channel_label=""):
    """Build and execute a single StackAndOverlayHistograms call."""
    input_arg = ",".join(input_lists)
    scale_arg = ",".join(scale_configs)

    os.makedirs(f"Histograms/{output_dir}", exist_ok=True)

    cmd = [
        EXEC,
        input_arg,
        COLOR_CONFIG,
        scale_arg,
        HIST_CONFIG,
        output_dir,
        lumi_text,
        "1" if USE_CATEGORY_MODE else "0",
        channel_label,
    ]

    print("Running command:")
    print(f"  {EXEC} \\")
    for arg in cmd[1:]:
        print(f"    {arg} \\")
    print()

    try:
        result = subprocess.run(cmd, check=True, text=True, capture_output=True)
        if result.stdout:
            print(result.stdout)
        print(f"  Done: Histograms/{output_dir}\n")
        return True
    except subprocess.CalledProcessError as e:
        print(f"  Error running analysis:")
        print(e.stderr)
        return False
    except FileNotFoundError:
        print(f"  Error: Executable '{EXEC}' not found.")
        print("  Make sure the executable exists and has execute permissions.")
        exit(1)


# ── 1. Per RunPeriod, per Channel (standard) ──────────────────────────────
print(f"\n{'='*70}")
print(f"[1] Per RunPeriod / Per Channel")
print(f"{'='*70}")

for RUNPERIOD in RUNPERIODS:
    for CHANNEL in CHANNELS:
        print(f"\n--- {STUDYNAME} / {RUNPERIOD} / {CHANNEL} ---")

        input_list   = get_input_list(RUNPERIOD, CHANNEL)
        scale_config = get_scale_config(RUNPERIOD, CHANNEL)

        if not os.path.exists(input_list):
            print(f"  Warning: Input list not found - {input_list}, skipping.")
            continue
        if not os.path.exists(scale_config):
            print(f"  Error: ScaleConfig not found - {scale_config}")
            print(f"  Please run Generate_ScaleConfig_From_DYSF.py first. Skipping.")
            continue

        output_dir = f"{STUDYNAME}_DYEstApplied{mode_suffix}/{RUNPERIOD}/{CHANNEL}"
        lumi_text  = LUMI_TEXTS.get(RUNPERIOD, f"{RUNPERIOD} 13 TeV")

        run_job([input_list], [scale_config], output_dir, lumi_text, CHANNEL)


# ── 2. Per RunPeriod, Dilepton (MuMu+ElEl+MuEl combined) ─────────────────
if RUN_CHANNEL_COMBINE:
    print(f"\n{'='*70}")
    print(f"[2] Per RunPeriod / Dilepton (channel combined)")
    print(f"{'='*70}")

    for RUNPERIOD in RUNPERIODS:
        print(f"\n--- {STUDYNAME} / {RUNPERIOD} / Dilepton ---")

        input_lists   = []
        scale_configs = []
        for CH in ["MuMu", "ElEl", "MuEl"]:
            il = get_input_list(RUNPERIOD, CH)
            sc = get_scale_config(RUNPERIOD, CH)
            if not os.path.exists(il):
                print(f"  Warning: Input list not found - {il}, skipping channel {CH}.")
                continue
            if not os.path.exists(sc):
                print(f"  Error: ScaleConfig not found - {sc}, skipping channel {CH}.")
                continue
            input_lists.append(il)
            scale_configs.append(sc)

        if not input_lists:
            print(f"  No valid channels found for {RUNPERIOD}, skipping.")
            continue

        output_dir = f"{STUDYNAME}_DYEstApplied{mode_suffix}/{RUNPERIOD}/Dilepton"
        lumi_text  = LUMI_TEXTS.get(RUNPERIOD, f"{RUNPERIOD} 13 TeV")

        run_job(input_lists, scale_configs, output_dir, lumi_text, "Dilepton")


# ── 3. Run2 combined, per Channel ─────────────────────────────────────────
if RUN_RUN2_COMBINE:
    print(f"\n{'='*70}")
    print(f"[3] Run2 / Per Channel (all RunPeriods combined)")
    print(f"{'='*70}")

    for CHANNEL in CHANNELS:
        print(f"\n--- {STUDYNAME} / Run2 / {CHANNEL} ---")

        input_lists   = []
        scale_configs = []
        for RP in RUNPERIODS:
            il = get_input_list(RP, CHANNEL)
            sc = get_scale_config(RP, CHANNEL)
            if not os.path.exists(il):
                print(f"  Warning: Input list not found - {il}, skipping {RP}.")
                continue
            if not os.path.exists(sc):
                print(f"  Error: ScaleConfig not found - {sc}, skipping {RP}.")
                continue
            input_lists.append(il)
            scale_configs.append(sc)

        if not input_lists:
            print(f"  No valid run periods found for {CHANNEL}, skipping.")
            continue

        output_dir = f"{STUDYNAME}_DYEstApplied{mode_suffix}/Run2/{CHANNEL}"
        lumi_text  = LUMI_TEXTS["Run2"]

        run_job(input_lists, scale_configs, output_dir, lumi_text, CHANNEL)


# ── 4. Run2 + Dilepton (everything combined) ──────────────────────────────
if RUN_RUN2_DILEPTON:
    print(f"\n{'='*70}")
    print(f"[4] Run2 / Dilepton (all RunPeriods + all channels combined)")
    print(f"{'='*70}")

    print(f"\n--- {STUDYNAME} / Run2 / Dilepton ---")

    input_lists   = []
    scale_configs = []
    for RP in RUNPERIODS:
        for CH in ["MuMu", "ElEl", "MuEl"]:
            il = get_input_list(RP, CH)
            sc = get_scale_config(RP, CH)
            if not os.path.exists(il):
                print(f"  Warning: Input list not found - {il}, skipping.")
                continue
            if not os.path.exists(sc):
                print(f"  Error: ScaleConfig not found - {sc}, skipping.")
                continue
            input_lists.append(il)
            scale_configs.append(sc)

    if input_lists:
        output_dir = f"{STUDYNAME}_DYEstApplied{mode_suffix}/Run2/Dilepton"
        lumi_text  = LUMI_TEXTS["Run2"]
        run_job(input_lists, scale_configs, output_dir, lumi_text, "Dilepton")
    else:
        print("  No valid inputs found, skipping.")


print(f"\n{'='*70}")
print(f"All jobs processed!")
print(f"{'='*70}")
