#!/usr/bin/env python3
"""
Generate_ScaleConfig_From_DYSF.py

Reads DYSF_Run2UL.txt and generates per-RunPeriod per-Channel ScaleConfig files.

Step -> DYSF selection mapping:
  _0          -> 1.0          (no selection)
  _1, _2      -> Dilepton
  _3, _4      -> >= 2 jets
  _5~_9, none -> >= 1 b-tag
"""

import os
import re

# Input
DYSF_FILE   = "DYSF_Run2UL.txt"
STUDYNAME   = "AN_v6p2-13"
STUDYNAME   = "AN_v6p4"
OUTPUT_BASE = "ScaleConfig"

# Step -> DYSF selection key mapping
# None = suffix-less histograms (h_Toppt, h_Reco_CPO*, etc.)
STEP_TO_SELECTION = {
    0:    "1.0",
    1:    "Dilepton",
    2:    "Dilepton",
    3:    ">= 2 jets",
    4:    ">= 2 jets",
    5:    ">= 1 b-tag",
    6:    ">= 1 b-tag",
    7:    ">= 1 b-tag",
    8:    ">= 1 b-tag",
    9:    ">= 1 b-tag",
    None: ">= 1 b-tag",
}

# Non-DY samples (always SF = 1.0)
NON_DY_SAMPLES = [
    "Data",
    "TTbar_Signal",
    "TTbar_AllHadronic",
    "TTbar_AllHadron",
    "TTbar_SemiLeptonic",
    "TTbar_SemiLepton",
    "ST_t-channel_top_4f_InclusiveDecays",
    "ST_t-channel_antitop_4f_InclusiveDecays",
    "ST_tW_top_5f_NoFullyHadronicDecays",
    "ST_tW_top_5f_DS_NoFullyHadronicDecays",
    "ST_tW_antitop_5f_NoFullyHadronicDecays",
    "ST_tW_antitop_5f_DS_NoFullyHadronicDecays",
    "ST_s-channel_4f_leptonDecays",
    "TTZToQQ",
    "TTZToLLNuNu",
    "TTZToLLNuNu_M-10",
    "TTWJetsToLNu",
    "TTWJetsToQQ",
    "WJetsToLNu",
    "WW",
    "WZ",
    "ZZ",
]

# Parse DYSF file using whole-line regex (selection can contain spaces)
def parse_dysf(filepath):
    """
    Returns dict: dysf[runperiod][selection][channel] = (SF, SF_err)
    """
    line_re = re.compile(
        r'^(UL2016PreVFP|UL2016PostVFP|UL2017|UL2018)_(.+?)_(MuMu|ElEl|MuEl)\s+'
        r'([0-9.eE+-]+)\s+([0-9.eE+-]+)'
    )
    dysf = {}
    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = line_re.match(line)
            if not m:
                print(f"  WARNING: Cannot parse line: {line}")
                continue
            runperiod = m.group(1)
            selection = m.group(2)   # "Dilepton", ">= 2 jets", ">= 1 b-tag"
            channel   = m.group(3)
            sf        = float(m.group(4))
            sf_err    = float(m.group(5))
            dysf.setdefault(runperiod, {}).setdefault(selection, {})[channel] = (sf, sf_err)
    return dysf


def get_dy_sf(dysf, runperiod, channel, step):
    """Return (SF, SF_err) for DY given step (int or None)."""
    selection = STEP_TO_SELECTION.get(step, ">= 1 b-tag")
    if selection == "1.0":
        return 1.0, 0.0
    try:
        return dysf[runperiod][selection][channel]
    except KeyError:
        print(f"  WARNING: SF not found for {runperiod}/{selection}/{channel}, using 1.0")
        return 1.0, 0.0


def write_scale_config(outpath, dysf, runperiod, channel):
    os.makedirs(os.path.dirname(outpath), exist_ok=True)

    all_steps = list(range(10)) + [None]

    with open(outpath, "w") as f:
        f.write(f"# ScaleConfig for {runperiod} {channel}\n")
        f.write(f"# Generated from DYSF_Run2UL.txt\n")
        f.write(f"#\n")
        f.write(f"# Format:\n")
        f.write(f"#   Non-DY:  <SampleName>  <SF>\n")
        f.write(f"#   DY:      DY  <step|*>  <SF>   (step * = suffix-less histograms)\n")
        f.write(f"#\n")
        f.write(f"# Step mapping:\n")
        f.write(f"#   step 0    -> 1.0 (no selection)\n")
        f.write(f"#   step 1,2  -> Dilepton\n")
        f.write(f"#   step 3,4  -> >= 2 jets\n")
        f.write(f"#   step 5~9  -> >= 1 b-tag\n")
        f.write(f"#   step *    -> >= 1 b-tag (suffix-less: h_Toppt, h_Reco_CPO*, ...)\n")
        f.write(f"#\n")

        # Non-DY samples
        f.write(f"# Non-DY samples\n")
        for sample in NON_DY_SAMPLES:
            f.write(f"{sample:<55s}  1.0\n")

        f.write(f"#\n")
        f.write(f"# DY step-wise SF ({runperiod} {channel})\n")

        prev_sel = None
        for step in all_steps:
            selection = STEP_TO_SELECTION.get(step, ">= 1 b-tag")
            sf, sf_err = get_dy_sf(dysf, runperiod, channel, step)
            step_str = str(step) if step is not None else "*"

            if selection != prev_sel:
                sel_label = "1.0 (fixed)" if selection == "1.0" else selection
                f.write(f"# {sel_label}\n")
                prev_sel = selection

            f.write(f"DY  {step_str:<4s}  {sf:.6f}  # sf_err={sf_err:.6f}\n")

    print(f"  Written: {outpath}")


def main():
    print(f"Parsing {DYSF_FILE} ...")
    dysf = parse_dysf(DYSF_FILE)

    # Print parsed keys for verification
    print("Parsed keys:")
    for rp in sorted(dysf):
        for sel in sorted(dysf[rp]):
            for ch in sorted(dysf[rp][sel]):
                sf, sf_err = dysf[rp][sel][ch]
                print(f"  {rp:20s}  {sel:15s}  {ch:5s}  SF={sf:.6f}")

    runperiods = ["UL2016PreVFP", "UL2016PostVFP", "UL2017", "UL2018"]
    channels   = ["MuMu", "ElEl", "MuEl"]

    for rp in runperiods:
        for ch in channels:
            outpath = f"{OUTPUT_BASE}/{STUDYNAME}/{rp}/{ch}/ScaleConfig_{STUDYNAME}_{rp}_{ch}.txt"
            print(f"\n[{rp} / {ch}]")
            write_scale_config(outpath, dysf, rp, ch)

    print(f"\nDone! ScaleConfig files written under {OUTPUT_BASE}/")


if __name__ == "__main__":
    main()
