# SSBControlPlotter

A lightweight ROOT-based plotting framework to visualize histograms with configurable styling, stack handling, and data/MC comparison — primarily for use in CMS-style control plot production.

---

## 📂 Project Structure

```
SSBControlPlotter/
├── input/                       # Directory for ROOT input files (user-provided)
├── CMS_lumi.h                  # CMS official style header for luminosity label
├── tdrstyle.h                  # TDR (Technical Design Report) plot styling
├── StackAndOverlayHistograms.cpp  # Main C++ script to stack, overlay, and plot histograms
├── run.sh                      # Shell script to compile and execute the plotter
├── Makefile                    # Makefile for building the plotter executable
├── ColorConfig.txt             # Configuration for color schemes
├── HistConfig.txt              # Configuration for histogram groups and plotting rules
├── ScaleConfig.txt             # Configuration for scaling histograms (e.g. lumi normalization)
├── README.md                   # This file
```

---

## Dependencies

- [ROOT](https://root.cern/) ≥ 6.xx
- C++17 compatible compiler
- Make / Bash

---

## Usage

1. **Build the executable**  
   Run the following command in the project root:
   ```bash
   make -f Makefile
   ```

2. **Run the plotting**  
   Use the provided shell script:
   ```bash
   ./run.sh
   ```

3. **Check output plots**  
   Output will be saved to the working directory or a specified subfolder.

---
