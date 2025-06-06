#!/bin/bash

# Define the executable and input files
EXEC="./StackAndOverlayHistograms"
#INPUT_LIST="input/Testv4p1/UL2016PostVFP/MuMu/Testv4p1_UL2016PostVFP_MuMu.list"
#INPUT_LIST="input/NanoAOD_v1/UL2018/MuMu/NanoAOD_v1_UL2018_MuMu.list"
INPUT_LIST="input/NanoAOD_v1p2/UL2018/MuMu/NanoAOD_v1p2_UL2018_MuMu.list"
INPUT_LIST="input/NanoAOD_v1p2/UL2016PreVFP/MuMu/NanoAOD_v1_UL2016PreVFP_MuMu.list"
INPUT_LIST="input/NanoAOD_v1/UL2016PreVFP/MuMu/NanoAOD_v1_UL2016PreVFP_MuMu.list"
COLOR_CONFIG="ColorConfig.txt"
SCALE_CONFIG="ScaleConfig.txt"
HIST_CONFIG="HistConfig.txt"
OUTPUT_DIR="NanoAOD_v1/UL2018/MuMu"
OUTPUT_DIR="NanoAOD_v1p2/UL2018/MuMu"
OUTPUT_DIR="NanoAOD_v1/UL2016PreVFP/MuMu"
LUMI_TEXT="UL 2018 13 TeV"        # Add luminosity text
LUMI_TEXT="UL 2016APV 13 TeV"     # Add luminosity text
LUMI_TEXT="59.83 fb^{-1} (13 TeV)" # Add luminosity text 2018
LUMI_TEXT="19.65 fb^{-1} (13 TeV)" # Add luminosity text 2016 PreVFP

# Run the executable with the provided arguments
echo $EXEC $INPUT_LIST $COLOR_CONFIG $SCALE_CONFIG $OUTPUT_DIR \"$LUMI_TEXT\"
$EXEC $INPUT_LIST $COLOR_CONFIG $SCALE_CONFIG $HIST_CONFIG $OUTPUT_DIR "$LUMI_TEXT" 
