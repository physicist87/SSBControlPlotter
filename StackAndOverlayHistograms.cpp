#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <THStack.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TColor.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TStyle.h>
#include <TKey.h>
#include <TPad.h>
#include <TLine.h>
#include <TLatex.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <algorithm>

// Include external header files
#include "tdrstyle.h"
#include "CMS_lumi.h"

// Function to parse the color configuration
// Register CMS official color palette (call once at startup)
void registerCMSColors() {
    // Only register if not already registered
    if (gROOT->GetColor(10001)) return;
    new TColor(10001,  63./255., 144./255., 218./255.);  // Azure Blue
    new TColor(10002, 255./255., 169./255.,  14./255.);  // Orange Yellow
    new TColor(10003, 189./255.,  31./255.,   1./255.);  // Deep Red
    new TColor(10004, 148./255., 164./255., 162./255.);  // Grey Green
    new TColor(10005, 131./255.,  45./255., 182./255.);  // Purple
    new TColor(10006, 169./255., 107./255.,  89./255.);  // Brown
    new TColor(10007, 231./255.,  99./255.,   0./255.);  // Deep Orange
    new TColor(10008, 185./255., 172./255., 112./255.);  // Olive
    new TColor(10009, 113./255., 117./255., 129./255.);  // Grey
    new TColor(10010, 146./255., 218./255., 221./255.);  // Teal
}

std::map<std::string, int> loadColorConfig(const std::string &colorConfigFile) {
    registerCMSColors();

    std::map<std::string, int> colorMap;
    std::ifstream infile(colorConfigFile);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open color config file." << std::endl;
        return colorMap;
    }

    std::string line;
    while (std::getline(infile, line)) {
        // Strip inline comments
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string name, colorToken;
        if (!(iss >> name >> colorToken)) continue;

        int color = kBlack;

        // Check if colorToken is a plain integer (CMS color index e.g. 10003)
        bool isNumeric = !colorToken.empty() &&
                         std::all_of(colorToken.begin(), colorToken.end(), ::isdigit);
        if (isNumeric) {
            color = std::stoi(colorToken);
        } else {
            // Legacy format: kName +offset
            int colorOffset = 0;
            std::string plusStr;
            iss >> plusStr >> colorOffset;

            if      (colorToken == "kRed")     color = kRed     + colorOffset;
            else if (colorToken == "kBlue")    color = kBlue    + colorOffset;
            else if (colorToken == "kGreen")   color = kGreen   + colorOffset;
            else if (colorToken == "kMagenta") color = kMagenta + colorOffset;
            else if (colorToken == "kYellow")  color = kYellow  + colorOffset;
            else if (colorToken == "kOrange")  color = kOrange  + colorOffset;
            else if (colorToken == "kAzure")   color = kAzure   + colorOffset;
            else if (colorToken == "kCyan")    color = kCyan    + colorOffset;
            else if (colorToken == "kWhite")   color = kWhite   + colorOffset;
            else if (colorToken == "kBlack")   color = kBlack   + colorOffset;
            else {
                std::cerr << "Warning: Unknown color name: " << colorToken << std::endl;
            }
        }

        std::cout << "ColorConfig: " << name << " -> " << color << std::endl;
        colorMap[name] = color;
    }

    infile.close();
    return colorMap;
}
// ScaleMap: scaleMap[sampleOrCategory][stepStr] = SF
// stepStr: "0"~"9" for histogram suffix _0~_9, "*" for suffix-less histograms
// Non-DY samples: scaleMap[sampleName]["*"] = 1.0
// DY category:    scaleMap["DY"][stepStr]   = SF
using ScaleMap = std::map<std::string, std::map<std::string, double>>;

ScaleMap loadScaleConfig(const std::string &scaleConfigFile) {
    ScaleMap scaleMap;
    std::ifstream infile(scaleConfigFile);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open scale config file." << std::endl;
        return scaleMap;
    }

    std::string line;
    while (std::getline(infile, line)) {
        // Strip inline comments
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string first;
        if (!(iss >> first)) continue;

        if (first == "DY") {
            // DY  <step|*>  <SF>
            std::string stepStr;
            double sf;
            if (!(iss >> stepStr >> sf)) {
                std::cerr << "Error: Invalid DY line in scale config: " << line << std::endl;
                continue;
            }
            scaleMap["DY"][stepStr] = sf;
            std::cout << "DY step=" << stepStr << " SF=" << sf << std::endl;
        } else {
            // <SampleName>  <SF>
            double sf;
            if (!(iss >> sf)) {
                std::cerr << "Error: Invalid format in scale config: " << line << std::endl;
                continue;
            }
            scaleMap[first]["*"] = sf;
            std::cout << "Sample=" << first << " SF=" << sf << std::endl;
        }
    }

    infile.close();
    return scaleMap;
}

// Extract step string from histogram name
// e.g. "h_Lep1pt_3" -> "3", "h_Reco_CPO1" -> "*"
std::string getHistStep(const std::string &histName) {
    // Find last underscore followed only by digits till end
    auto pos = histName.rfind('_');
    if (pos != std::string::npos) {
        std::string suffix = histName.substr(pos + 1);
        if (!suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
            return suffix;
        }
    }
    return "*";
}

// Get SF for a given sample and histogram name
double getSF(const ScaleMap &scaleMap, const std::string &sampleName,
             const std::string &category, const std::string &histName) {
    std::string step = getHistStep(histName);

    // DY category: step-wise SF
    if (category == "DY") {
        auto catIt = scaleMap.find("DY");
        if (catIt != scaleMap.end()) {
            auto sfIt = catIt->second.find(step);
            if (sfIt != catIt->second.end()) return sfIt->second;
            // fallback to *
            auto defIt = catIt->second.find("*");
            if (defIt != catIt->second.end()) return defIt->second;
        }
        std::cerr << "Warning: DY SF not found for step=" << step << ", using 1.0" << std::endl;
        return 1.0;
    }

    // Non-DY: sample-level SF (stored under "*")
    auto sIt = scaleMap.find(sampleName);
    if (sIt != scaleMap.end()) {
        auto sfIt = sIt->second.find("*");
        if (sfIt != sIt->second.end()) return sfIt->second;
    }
    std::cerr << "Warning: SF not found for sample=" << sampleName << ", using 1.0" << std::endl;
    return 1.0;
}

struct HistConfig {
    int rebinFactor;
    std::string xAxisLabel;
};

std::map<std::string, HistConfig> loadHistConfig(const std::string &histConfigFile) {
    std::map<std::string, HistConfig> histConfigMap;
    std::ifstream infile(histConfigFile);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open histogram config file." << std::endl;
        return histConfigMap;
    }

    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string histNamePattern;
        int rebinFactor;
        std::string xAxisLabel;

        if (!(iss >> histNamePattern >> rebinFactor)) {
            std::cerr << "Error: Invalid format in histogram config file: " << line << std::endl;
            continue;
        }
        
        std::string restOfLine;
        std::getline(iss, restOfLine);
        restOfLine.erase(0, restOfLine.find_first_not_of(" \t"));
        xAxisLabel = restOfLine;
        
        size_t commentPos = xAxisLabel.find("//");
        if (commentPos != std::string::npos) {
            xAxisLabel = xAxisLabel.substr(0, commentPos);
            xAxisLabel.erase(xAxisLabel.find_last_not_of(" \t") + 1);
        }
        
        std::string processedLabel = "";
        for (size_t i = 0; i < xAxisLabel.length(); ++i) {
            if (xAxisLabel[i] == '\\' && i + 1 < xAxisLabel.length()) {
                if (xAxisLabel[i+1] == '\\') {
                    processedLabel += ' ';
                    i++;
                } else {
                    processedLabel += xAxisLabel[i];
                }
            } else {
                processedLabel += xAxisLabel[i];
            }
        }

        std::cout << "histNamePattern: " << histNamePattern 
                  << " rebinFactor: " << rebinFactor 
                  << " xAxisLabel: " << processedLabel << std::endl;
        
        histConfigMap[histNamePattern] = {rebinFactor, processedLabel};
    }

    infile.close();
    return histConfigMap;
}

bool matchesPattern(const std::string &histName, const std::string &pattern) {
    return histName.find(pattern) != std::string::npos;
}

void applyHistConfig(TH1 *hist, const std::map<std::string, HistConfig> &histConfigMap) {
    if (!hist) return;
    
    std::string histName = hist->GetName();
    
    for (const auto &configPair : histConfigMap) {
        const std::string &pattern = configPair.first;
        const HistConfig &config = configPair.second;
        
        if (matchesPattern(histName, pattern)) {
            std::cout << "Applying config to " << histName << ": Rebin=" << config.rebinFactor 
                      << ", XAxisLabel=" << config.xAxisLabel << std::endl;
            
            if (config.rebinFactor > 1) {
                hist->Rebin(config.rebinFactor);
            }
            
            if (!config.xAxisLabel.empty()) {
                hist->GetXaxis()->SetTitle(config.xAxisLabel.c_str());
            }
            
            break;
        }
    }
}




std::string getCategoryName(const std::string &sampleName) {
    // Data
    if (sampleName == "Data") {
        return "Data";
    }
    // DY samples
    if (sampleName.find("DYJetsToLL") != std::string::npos || 
        sampleName.find("DYJets") != std::string::npos) {
        return "DY";
    }
    // TTV samples - Check BEFORE WJets to catch TTW and TTZ
    if (sampleName.find("TTZ") != std::string::npos || 
        sampleName.find("TTW") != std::string::npos) {
        return "TTV";
    }
    // W+jets samples - Check AFTER TTV
    // Make sure it's not TTW by checking it doesn't start with TT
    if ((sampleName.find("WJetsToLNu") != std::string::npos ||
         sampleName.find("WJets") != std::string::npos) &&
        sampleName.find("TT") == std::string::npos) {
        return "WJets";
    }
    // Diboson samples
    if (sampleName == "WW" || sampleName == "WZ" || sampleName == "ZZ") {
        return "Diboson";
    }
    // SingleTop samples
    if (sampleName.find("ST_") != std::string::npos) {
        return "SingleTop";
    }
    // TTbar Signal
    if (sampleName == "TTbar_Signal") {
        return "TTbar_Signal";
    }
    // TTbar Others (AllHadronic, SemiLeptonic, etc.)
    if (sampleName.find("TTbar") != std::string::npos) {
        return "TTbar_Others";
    }
    
    // If still unknown, print warning
    std::cerr << "WARNING: Unknown sample name: " << sampleName << std::endl;
    
    return "Unknown";
}

int getCategoryOrder(const std::string &category) {
    if (category == "TTbar_Signal") return 1;
    if (category == "TTbar_Others") return 2;
    if (category == "DY") return 3;
    if (category == "SingleTop") return 4;
    if (category == "TTV") return 5;
    if (category == "WJets") return 6;
    if (category == "Diboson") return 7;
    return 999;
}

double GetHistogramMaxWithMargin(TH1* hist, double marginFactor = 1.2) {
    if (!hist) return 0;
    double maxVal = hist->GetMaximum();
    return maxVal * marginFactor;
}

// Write event yield table (both category and individual mode) to EventYield.txt
// Uses h_Num_PV_0 ~ h_Num_PV_8 (and h_Num_PV without suffix) as event counters per step.
void writeEventYieldTable(
    const std::string &outputDir,
    const std::map<std::string, std::map<std::string, std::unique_ptr<TH1>>> &histograms,
    const std::map<std::string, std::unique_ptr<TH1>> &dataHistograms)
{
    // Each entry stores { yield, stat_error }
    struct YE {
        double yield = 0;
        double err2  = 0;    // accumulated squared error (for MC: Sumw2, for Data: N)
        double err   = 0;    // sqrt(err2), filled after accumulation
    };

    std::map<std::string, std::map<std::string, YE>> catYE;   // category -> stepStr -> YE
    std::map<std::string, std::map<std::string, YE>> smpYE;   // sample   -> stepStr -> YE
    std::map<std::string, YE>                         dataYE;  // stepStr  -> YE

    // Step keys: *, 0, 1, ..., 8
    std::vector<std::string> stepKeys;
    stepKeys.push_back("*");
    for (int s = 0; s <= 8; ++s) stepKeys.push_back(std::to_string(s));

    auto stepToHistName = [](const std::string &step) -> std::string {
        if (step == "*") return "h_Num_PV";
        return "h_Num_PV_" + step;
    };

    // Helper: integrate TH1 and compute stat error via Sumw2
    // Returns {integral, sqrt(sum of bin_error^2)} over all bins including overflow
    auto integrateWithError = [](TH1 *h, bool isData) -> std::pair<double,double> {
        double yield = 0, err2 = 0;
        int nBins = h->GetNbinsX();
        for (int b = 0; b <= nBins + 1; ++b) {  // include underflow/overflow
            double w = h->GetBinContent(b);
            yield += w;
            if (isData) {
                // weight=1 per event → Poisson: σ² = N
                err2 += w;
            } else {
                // MC with Sumw2: σ² = Σ w²  (stored in bin error as sqrt)
                double e = h->GetBinError(b);
                err2 += e * e;
            }
        }
        return {yield, std::sqrt(err2)};
    };

    // Collect MC yields per sample and per category
    for (const auto &samplePair : histograms) {
        const std::string &sampleName = samplePair.first;
        const std::string  category   = getCategoryName(sampleName);

        for (const std::string &step : stepKeys) {
            std::string hname = stepToHistName(step);
            auto it = samplePair.second.find(hname);
            if (it == samplePair.second.end()) continue;

            auto [y, e] = integrateWithError(it->second.get(), false);
            smpYE[sampleName][step].yield += y;
            smpYE[sampleName][step].err2  += e * e;   // add in quadrature across channels
            catYE[category][step].yield   += y;
            catYE[category][step].err2    += e * e;
        }
    }

    // Collect Data yields (weight=1 → Poisson)
    for (const std::string &step : stepKeys) {
        std::string hname = stepToHistName(step);
        auto it = dataHistograms.find(hname);
        if (it == dataHistograms.end()) continue;
        auto [y, e] = integrateWithError(it->second.get(), true);
        dataYE[step] = {y, e*e, e};
    }

    // Determine active steps
    std::vector<std::string> activeSteps;
    for (const std::string &step : stepKeys) {
        bool found = false;
        for (const auto &sp : smpYE) {
            auto it = sp.second.find(step);
            if (it != sp.second.end() && it->second.yield > 0) { found = true; break; }
        }
        if (found) activeSteps.push_back(step);
    }

    if (activeSteps.empty()) {
        std::cerr << "Warning: No h_Num_PV histograms found. EventYield.txt will be empty." << std::endl;
    }

    // Ordered category list
    std::vector<std::pair<int,std::string>> catOrder;
    for (const auto &cp : catYE) catOrder.push_back({getCategoryOrder(cp.first), cp.first});
    std::sort(catOrder.begin(), catOrder.end());
    std::vector<std::string> orderedCats;
    for (const auto &op : catOrder) orderedCats.push_back(op.second);

    // Ordered sample list
    std::vector<std::string> orderedSamples;
    for (const auto &sp : smpYE) orderedSamples.push_back(sp.first);
    std::reverse(orderedSamples.begin(), orderedSamples.end());

    // Helper: format yield value
    auto fmtVal = [](double v) -> std::string {
        char buf[64];
        if (v >= 1e6)      snprintf(buf, sizeof(buf), "%.0f", v);
        else if (v >= 1e3) snprintf(buf, sizeof(buf), "%.1f", v);
        else if (v >= 1.)  snprintf(buf, sizeof(buf), "%.2f", v);
        else               snprintf(buf, sizeof(buf), "%.4f", v);
        return std::string(buf);
    };

    // Helper: format "yield ± error"
    auto fmtYE = [&](double y, double e) -> std::string {
        return fmtVal(y) + " +/- " + fmtVal(e);
    };

    // Helper: format Data/MC ratio with propagated error
    // ratio = D/M,  σ_ratio = ratio * sqrt( (σ_D/D)² + (σ_M/M)² )
    auto fmtRatio = [](double d, double sd, double m, double sm) -> std::string {
        if (m <= 0) return "N/A";
        double ratio = d / m;
        double relD  = (d > 0) ? sd / d : 0;
        double relM  = (m > 0) ? sm / m : 0;
        double serr  = ratio * std::sqrt(relD*relD + relM*relM);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.4f +/- %.4f", ratio, serr);
        return std::string(buf);
    };

    // Step label
    auto stepLabel = [](const std::string &step) -> std::string {
        if (step == "*") return "No suffix";
        return "Step " + step;
    };

    // Column width: "yield +/- error" needs more space
    const int colW  = 26;   // wide enough for "123456.7 +/- 1234.5"
    const int labelW = 22;  // process name column
    auto padCol = [&](const std::string &s) -> std::string {
        if ((int)s.size() >= colW) return s + "  ";
        return s + std::string(colW - s.size(), ' ');
    };
    auto padLabel = [&](const std::string &s) -> std::string {
        if ((int)s.size() >= labelW) return s + "  ";
        return s + std::string(labelW - s.size(), ' ');
    };

    std::string yieldPath = "Histograms/" + outputDir + "/EventYield.txt";
    std::ofstream out(yieldPath.c_str());
    if (!out.is_open()) {
        std::cerr << "Error: Could not open EventYield.txt for writing." << std::endl;
        return;
    }

    // Reusable lambda: print one section (category or sample)
    // processNames: ordered list of row labels
    // yieldMap:     processName -> stepStr -> YE
    auto printSection = [&](
        const std::string &title,
        const std::vector<std::string> &processNames,
        std::map<std::string, std::map<std::string, YE>> &yieldMap)
    {
        out << "=== " << title << " ===" << std::endl;
        out << std::endl;

        // Pre-compute BG total (MC Total - TTbar_Signal) + error per step
        struct TotYE { double y = 0; double err2 = 0; };
        std::map<std::string, TotYE> mcTot;
        std::map<std::string, TotYE> bgTot;
        for (const std::string &step : activeSteps) {
            for (const std::string &proc : processNames) {
                auto it = yieldMap[proc].find(step);
                if (it == yieldMap[proc].end()) continue;
                mcTot[step].y    += it->second.yield;
                mcTot[step].err2 += it->second.err2;
                // BG = all except TTbar_Signal
                if (proc != "TTbar_Signal") {
                    bgTot[step].y    += it->second.yield;
                    bgTot[step].err2 += it->second.err2;
                }
            }
        }

        // Header
        out << padLabel("Process");
        for (const std::string &step : activeSteps) out << padCol(stepLabel(step));
        out << std::endl;
        out << std::string(labelW + activeSteps.size() * colW, '-') << std::endl;

        // One row per process
        for (const std::string &proc : processNames) {
            out << padLabel(proc);
            for (const std::string &step : activeSteps) {
                auto it = yieldMap[proc].find(step);
                double y = 0, e = 0;
                if (it != yieldMap[proc].end()) {
                    y = it->second.yield;
                    e = std::sqrt(it->second.err2);
                }
                out << padCol(fmtYE(y, e));
            }
            out << std::endl;
        }

        // BG Total row (MC Total - TTbar_Signal)
        out << padLabel("BG Total");
        for (const std::string &step : activeSteps) {
            double y = bgTot[step].y;
            double e = std::sqrt(bgTot[step].err2);
            out << padCol(fmtYE(y, e));
        }
        out << std::endl;

        // MC Total row
        out << padLabel("MC Total");
        for (const std::string &step : activeSteps) {
            double y = mcTot[step].y;
            double e = std::sqrt(mcTot[step].err2);
            out << padCol(fmtYE(y, e));
        }
        out << std::endl;

        // Data + Data/MC rows
        if (!dataYE.empty()) {
            out << padLabel("Data");
            for (const std::string &step : activeSteps) {
                auto dit = dataYE.find(step);
                double y = 0, e = 0;
                if (dit != dataYE.end()) { y = dit->second.yield; e = dit->second.err; }
                out << padCol(fmtYE(y, e));
            }
            out << std::endl;

            out << padLabel("Data/MC");
            for (const std::string &step : activeSteps) {
                auto dit = dataYE.find(step);
                double dy = 0, de = 0;
                if (dit != dataYE.end()) { dy = dit->second.yield; de = dit->second.err; }
                double my = mcTot[step].y;
                double me = std::sqrt(mcTot[step].err2);
                out << padCol(fmtRatio(dy, de, my, me));
            }
            out << std::endl;
        }

        out << std::endl << std::endl;
    };

    // ---------------------------------------------------------------
    // Section 1: Category Mode
    // ---------------------------------------------------------------
    printSection("Event Yield Table (Category Mode)", orderedCats, catYE);

    // ---------------------------------------------------------------
    // Section 2: Individual Sample Mode
    // ---------------------------------------------------------------
    printSection("Event Yield Table (Individual Sample Mode)", orderedSamples, smpYE);

    out.close();
    std::cout << "EventYield.txt written to: " << yieldPath << std::endl;
}

void savePlotsWithBothScales(TCanvas& canvas, TPad* pad1, const std::string& outputDir, 
                           const std::string& histName, TH1* mcSum, double maxY) {
    
    // Save Log scale
    std::string outputPathLog    = "Histograms/" + outputDir + "/" + histName + "_Log.pdf";
    std::string outputPathPngLog = "Histograms/" + outputDir + "/" + histName + "_Log.png";
    canvas.SaveAs(outputPathLog.c_str());
    canvas.SaveAs(outputPathPngLog.c_str());
    
    // Switch to Linear scale with a modest margin (2.0x) for legend space
    pad1->cd();
    pad1->SetLogy(0);
    
    THStack* stack = (THStack*)pad1->GetPrimitive(histName.c_str());
    if (stack) {
        double linearMax = mcSum ? mcSum->GetMaximum() * 2.0 : maxY;
        stack->SetMaximum(linearMax);
        stack->SetMinimum(0);
    }
    
    pad1->Modified();
    pad1->Update();
    
    std::string outputPathLinear    = "Histograms/" + outputDir + "/" + histName + "_Linear.pdf";
    std::string outputPathPngLinear = "Histograms/" + outputDir + "/" + histName + "_Linear.png";
    canvas.SaveAs(outputPathLinear.c_str());
    canvas.SaveAs(outputPathPngLinear.c_str());
    
    // Restore Log scale
    pad1->SetLogy(1);
    pad1->Modified();
    pad1->Update();
}

// Convert channel key to TLatex label
std::string getChannelLabel(const std::string &channel) {
    if (channel == "MuMu")     return "#mu^{+}#mu^{-} channel";
    if (channel == "ElEl")     return "e^{+}e^{-} channel";
    if (channel == "MuEl")     return "#mu^{#pm}e^{#mp} channel";
    if (channel == "Dilepton") return "l^{+}l^{-} channel";
    return channel;
}

// Progress bar utility
// Prints:  [tag] [████████░░░░░░░░] 50% (current/total) label
void printProgress(const std::string &tag, int current, int total, const std::string &label = "") {
    if (total <= 0) return;
    const int barWidth = 30;
    double frac = static_cast<double>(current) / total;
    int filled  = static_cast<int>(frac * barWidth);

    std::string bar;
    for (int i = 0; i < barWidth; ++i) bar += (i < filled ? "\u2588" : "\u2591");

    int pct = static_cast<int>(frac * 100);
    std::string msg = "[" + tag + "] [" + bar + "] "
                    + std::to_string(pct) + "% ("
                    + std::to_string(current) + "/" + std::to_string(total) + ")";
    if (!label.empty()) msg += "  " + label;

    // Pad to 100 chars so previous longer lines are fully overwritten
    if ((int)msg.size() < 100) msg += std::string(100 - msg.size(), ' ');

    std::cout << "\r" << msg << std::flush;
    if (current == total) std::cout << std::endl;  // newline when done
}

// Split a comma-separated string into tokens
std::vector<std::string> splitByComma(const std::string &s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

void StackAndOverlayHistograms(const std::string &inputFileList, const std::string &colorConfigFile, 
                               const std::string &scaleConfigFile, const std::string &histConfigFile,
                               const std::string &outputDir, const std::string &lumiText = "13 TeV",
                               bool useCategoryMode = false, const std::string &channelLabel = "") {
    // Support comma-separated multiple input lists and scale configs
    std::vector<std::string> inputFileLists  = splitByComma(inputFileList);
    std::vector<std::string> scaleConfigFiles = splitByComma(scaleConfigFile);

    if (inputFileLists.size() != scaleConfigFiles.size()) {
        std::cerr << "Error: number of input lists (" << inputFileLists.size()
                  << ") != number of scale configs (" << scaleConfigFiles.size() << ")" << std::endl;
        return;
    }

    std::cout << "Number of input list/ScaleConfig pairs: " << inputFileLists.size() << std::endl;
    std::cout << "Category Mode: " << (useCategoryMode ? "ON" : "OFF") << std::endl;

    setTDRStyle();
    gStyle->SetOptStat(0);

    std::map<std::string, int> colorMap = loadColorConfig(colorConfigFile);
    std::map<std::string, HistConfig> histConfigMap = loadHistConfig(histConfigFile);

    std::map<std::string, std::map<std::string, std::unique_ptr<TH1>>> histograms;
    std::map<std::string, std::unique_ptr<TH1>> dataHistograms;

    // Load each list with its corresponding ScaleConfig
    for (size_t pairIdx = 0; pairIdx < inputFileLists.size(); ++pairIdx) {
        const std::string &listFile   = inputFileLists[pairIdx];
        const std::string &scaleFile  = scaleConfigFiles[pairIdx];

        std::cout << "\n--- Pair " << pairIdx+1 << " ---" << std::endl;
        std::cout << "  List:        " << listFile  << std::endl;
        std::cout << "  ScaleConfig: " << scaleFile << std::endl;

        ScaleMap scaleMap = loadScaleConfig(scaleFile);

        std::ifstream fileList(listFile);
        if (!fileList.is_open()) {
            std::cerr << "Error: Could not open input file list: " << listFile << std::endl;
            continue;
        }

        // Pre-count files for progress bar
        std::vector<std::string> fileLines;
        std::string line;
        while (std::getline(fileList, line)) {
            if (line.empty() || line[0] == '#') continue;
            fileLines.push_back(line);
        }
        fileList.close();

        int nFiles = static_cast<int>(fileLines.size());
        int fileIdx = 0;
        std::cout << "[Loading] " << listFile << " (" << nFiles << " files)" << std::endl;

        for (const std::string &line : fileLines) {
            ++fileIdx;
            std::string sampleName = line.substr(line.find_last_of('/') + 1);
            sampleName = sampleName.substr(0, sampleName.find_first_of('.'));
            printProgress("Loading", fileIdx, nFiles, sampleName);

            TFile inputFile(line.c_str(), "READ");
            if (!inputFile.IsOpen()) {
                std::cerr << "\n[Loading] Error: Could not open file " << line << std::endl;
                continue;
            }

            TIter next(inputFile.GetListOfKeys());
            TKey *key;
            while ((key = (TKey *)next())) {
                std::unique_ptr<TObject> obj(key->ReadObj());
                if (obj && obj->InheritsFrom(TH1::Class())) {
                    std::string histName = obj->GetName();
                    std::unique_ptr<TH1> hist(dynamic_cast<TH1 *>(obj.release()));
                    hist->SetDirectory(0);

                    applyHistConfig(hist.get(), histConfigMap);

                    // Apply scale immediately so channels can be safely added together
                    if (sampleName != "Data") {
                        std::string category = getCategoryName(sampleName);
                        double sf = getSF(scaleMap, sampleName, category, histName);
                        hist->Scale(sf);
                    }

                    if (sampleName == "Data") {
                        if (dataHistograms.find(histName) == dataHistograms.end()) {
                            dataHistograms[histName] = std::move(hist);
                        } else {
                            dataHistograms[histName]->Add(hist.get());
                        }
                    } else {
                        if (histograms[sampleName].find(histName) == histograms[sampleName].end()) {
                            histograms[sampleName][histName] = std::move(hist);
                        } else {
                            histograms[sampleName][histName]->Add(hist.get());
                        }
                    }
                }
            }
            inputFile.Close();
        }
    }

    std::cout << "outputDir :" << outputDir << std::endl;
    std::cout << Form("mkdir -p Histograms/%s",outputDir.c_str())<< std::endl;
    gSystem->Exec(Form("mkdir -p Histograms/%s",outputDir.c_str()));

    std::string outputFileName = Form("Histograms/%s/Integral.txt",outputDir.c_str());
    std::ofstream integralFile(outputFileName.c_str());
    if (!integralFile.is_open()) {
        std::cerr << "Error: Could not open output file for integrals." << std::endl;
        return;
    }
    // Write event yield table (both sections) to EventYield.txt
    writeEventYieldTable(outputDir, histograms, dataHistograms);

    if (useCategoryMode) {
        std::cout << "=== Processing in Category Mode ===" << std::endl;
        
        std::map<std::string, std::map<std::string, std::unique_ptr<TH1>>> categoryHistograms;
        
        for (const auto &samplePair : histograms) {
            const std::string &sampleName = samplePair.first;
            std::string category = getCategoryName(sampleName);
            for (const auto &histPair : samplePair.second) {
                const std::string &histName = histPair.first;
                TH1* hist = histPair.second.get();
                if (categoryHistograms[category].find(histName) == categoryHistograms[category].end()) {
                    categoryHistograms[category][histName].reset((TH1*)hist->Clone());
                } else {
                    categoryHistograms[category][histName]->Add(hist);
                }
            }
        }

        int nHists  = static_cast<int>(categoryHistograms.begin()->second.size());
        int histIdx = 0;
        std::cout << "[Plotting] Category mode: " << nHists << " histograms" << std::endl;

        for (const auto &histPair : categoryHistograms.begin()->second) {
            ++histIdx;
            TCanvas canvas("canvas", "Histogram Stacks", 1200, 1200);
            canvas.cd();
            
            TPad *pad1 = new TPad("pad1", "pad1", 0, 0.3, 1, 1.0);
            pad1->SetBottomMargin(0.02);
            pad1->SetLeftMargin(0.16);
            pad1->SetRightMargin(0.05);
            pad1->SetTopMargin(0.1);
            pad1->SetLogy(1);
            pad1->Draw();
            
            TPad *pad2 = new TPad("pad2", "pad2", 0, 0.0, 1, 0.3);
            pad2->SetTopMargin(0.03);
            pad2->SetBottomMargin(0.35);
            pad2->SetLeftMargin(0.16);
            pad2->SetRightMargin(0.05);
            pad2->Draw();
            
            const std::string &histName = histPair.first;
            auto stack = std::make_unique<THStack>(histName.c_str(), "");
            
            auto legend = std::make_unique<TLegend>(0.40, 0.70, 0.94, 0.88);
            legend->SetBorderSize(0);
            legend->SetFillStyle(0);
            legend->SetTextFont(42);
            legend->SetTextSize(0.035);
            legend->SetMargin(0.15);
            legend->SetNColumns(2);
            
            if (histName.find("h_Num_PV") != std::string::npos) {
                integralFile << histName << std::endl << std::endl;
            }
            
            TH1 *mcSum = nullptr;
            double inteMCtotal = 0;
            
            std::vector<std::pair<int, std::string>> categoryOrder;
            for (const auto &catPair : categoryHistograms) {
                categoryOrder.push_back({getCategoryOrder(catPair.first), catPair.first});
            }
            std::sort(categoryOrder.begin(), categoryOrder.end());
            std::reverse(categoryOrder.begin(), categoryOrder.end());
            for (const auto &orderPair : categoryOrder) {
                const std::string &category = orderPair.second;
                
                auto histIt = categoryHistograms[category].find(histName);
                if (histIt == categoryHistograms[category].end()) continue;
                
                TH1 *hist = histIt->second.get();
                
                if (!mcSum) {
                    mcSum = (TH1*)hist->Clone("mcSum");
                    mcSum->Reset();
                }
                
                if (colorMap.find(category) != colorMap.end()) {
                    hist->SetFillColor(colorMap.at(category));
                } else {
                    std::cerr << "Warning: color not found for category " << category << std::endl;
                }

                hist->SetLineColor(kBlack);
                hist->SetLineWidth(1);
                hist->SetFillStyle(1001);
                
                mcSum->Add(hist);
                stack->Add(hist);
                legend->AddEntry(hist, Form("%s", category.c_str()), "f");
                
                if (histName.find("h_Num_PV") != std::string::npos) {
                    integralFile << category << " " << hist->Integral() << std::endl;
                }
                inteMCtotal += hist->Integral();
            }
            
            if (histName.find("h_Num_PV") != std::string::npos) {
                integralFile << "MCtotal:  " << inteMCtotal << std::endl;
            }
            
            pad1->cd();
            
            TH1 *ratioHist = nullptr;
            double maxY = 0;
            
            if (mcSum) {
                maxY = GetHistogramMaxWithMargin(mcSum, 50.);
            }
            
            if (dataHistograms.find(histName) != dataHistograms.end()) {
                TH1 *dataHist = dataHistograms[histName].get();
                double dataMax = GetHistogramMaxWithMargin(dataHist, 50.);
                if (dataMax > maxY) {
                    maxY = dataMax;
                }
            }
            stack->Draw("HIST");
            stack->SetMaximum(maxY);
            
            stack->GetXaxis()->SetLabelSize(0);
            stack->GetXaxis()->SetTitleSize(0);
            stack->GetYaxis()->SetTitle("Events");
            stack->GetYaxis()->SetTitleSize(0.06);
            stack->GetYaxis()->SetTitleOffset(1.1);
            stack->GetYaxis()->SetLabelSize(0.05);
            
            if (dataHistograms.find(histName) != dataHistograms.end()) {
                TH1 *dataHist = dataHistograms[histName].get();
                dataHist->SetMarkerStyle(20);
                dataHist->SetMarkerSize(1.2);
                dataHist->SetMarkerColor(kBlack);
                dataHist->SetLineColor(kBlack);
                dataHist->Draw("SAME Ex0");
                legend->AddEntry(dataHist, Form("Data"), "APE");
                
                if (histName.find("h_Num_PV") != std::string::npos) {
                    integralFile << "Data  " << dataHist->Integral() << std::endl;
                    integralFile << "Frac(Data/MC)  " << dataHist->Integral()/inteMCtotal << std::endl;
                }
                
                ratioHist = (TH1*)dataHist->Clone("ratioHist");
                ratioHist->SetTitle("");
                ratioHist->Divide(mcSum);
                ratioHist->SetMinimum(0.5);
                ratioHist->SetMaximum(1.5);
                
                if (histName.find("h_Num_PV") != std::string::npos) {
                    integralFile << std::endl;
                }
            }
            
            legend->Draw();
            //CMS_lumi(pad1, "Preliminary", lumiText.c_str());
            CMS_lumi(pad1, "Private Work", lumiText.c_str());
            if (!channelLabel.empty()) {
                TLatex *chLabel = new TLatex();
                chLabel->SetNDC();
                chLabel->SetTextFont(42);
                chLabel->SetTextSize(0.040);
                chLabel->SetTextAlign(13);
                chLabel->DrawLatex(0.22, 0.74, getChannelLabel(channelLabel).c_str());
            }
            
            if (ratioHist) {
                pad2->cd();
                
                ratioHist->SetStats(0);
                ratioHist->GetYaxis()->SetTitle("Data/MC");
                ratioHist->GetYaxis()->SetTitleSize(0.12);
                ratioHist->GetYaxis()->SetTitleOffset(0.5);
                ratioHist->GetYaxis()->SetLabelSize(0.1);
                ratioHist->GetYaxis()->SetNdivisions(505);
                
                ratioHist->GetXaxis()->SetLabelSize(0.12);
                ratioHist->GetXaxis()->SetTitleSize(0.12);
                ratioHist->GetXaxis()->SetTitleOffset(1.0);
                ratioHist->GetXaxis()->SetTitle(mcSum->GetXaxis()->GetTitle());
                
                ratioHist->Draw("E1P");
                
                TLine *line = new TLine(ratioHist->GetXaxis()->GetXmin(), 1.0, 
                                       ratioHist->GetXaxis()->GetXmax(), 1.0);
                line->SetLineStyle(2);
                line->SetLineColor(kRed);
                line->SetLineWidth(2);
                line->Draw();
            }
            
            savePlotsWithBothScales(canvas, pad1, outputDir, histName, mcSum, maxY);
            printProgress("Plotting", histIdx, nHists, histName);
            
            canvas.Clear();
            delete ratioHist;
            delete mcSum;
        }
    }
    else {
        int nHistsIndv  = static_cast<int>(histograms.begin()->second.size());
        int histIdxIndv = 0;
        std::cout << "[Plotting] Individual mode: " << nHistsIndv << " histograms" << std::endl;

        for (const auto &histPair : histograms.begin()->second) {
            ++histIdxIndv;
            TCanvas canvas("canvas", "Histogram Stacks", 1200, 1200);
            canvas.cd();
            
            TPad *pad1 = new TPad("pad1", "pad1", 0, 0.3, 1, 1.0);
            pad1->SetBottomMargin(0.02);
            pad1->SetLeftMargin(0.16);
            pad1->SetRightMargin(0.05);
            pad1->SetTopMargin(0.1);
            pad1->SetLogy(1);
            pad1->Draw();
            
            TPad *pad2 = new TPad("pad2", "pad2", 0, 0.0, 1, 0.3);
            pad2->SetTopMargin(0.03);
            pad2->SetBottomMargin(0.35);
            pad2->SetLeftMargin(0.16);
            pad2->SetRightMargin(0.05);
            pad2->Draw();
            
            const std::string &histName = histPair.first;
            auto stack = std::make_unique<THStack>(histName.c_str(), "");
            
            auto legend = std::make_unique<TLegend>(0.40, 0.70, 0.94, 0.88);
            legend->SetBorderSize(0);
            legend->SetFillStyle(0);
            legend->SetTextFont(42);
            legend->SetTextSize(0.035);
            legend->SetMargin(0.15);
            legend->SetNColumns(2);
            
            if (histName.find("h_Num_PV") != std::string::npos) {
                integralFile << histName << std::endl << std::endl;
            }
            
            TH1 *mcSum = nullptr;
            double inteMCtotal = 0;
            
            std::vector<std::string> sampleOrder;
            for (const auto &samplePair : histograms) {
                sampleOrder.push_back(samplePair.first);
            }
            std::reverse(sampleOrder.begin(), sampleOrder.end());
            for (const auto &sampleName : sampleOrder) {
                auto &samplePair = histograms[sampleName];

                auto histIt = samplePair.find(histName);
                if (histIt == samplePair.end()) {
                    std::cerr << "Warning: histogram " << histName << " not found for sample " << sampleName << std::endl;
                    continue;
                }
                TH1 *hist = histIt->second.get();
                
                if (!mcSum) {
                    mcSum = (TH1*)hist->Clone("mcSum");
                    mcSum->Reset();
                }
                
                {
                    std::string indvCat = getCategoryName(sampleName);
                    if (colorMap.find(indvCat) != colorMap.end()) {
                        hist->SetLineColor(kBlack);
                        hist->SetLineWidth(1);
                        hist->SetFillColor(colorMap.at(indvCat));
                    } else {
                        std::cerr << "Warning: color not found for category " << indvCat
                                  << " (sample: " << sampleName << ")" << std::endl;
                    }
                }
                
                hist->SetFillStyle(1001);
                
                // Scale already applied at load time
                mcSum->Add(hist);
                stack->Add(hist);
                legend->AddEntry(hist, Form("%s", sampleName.c_str()), "f");
                
                if (histName.find("h_Num_PV") != std::string::npos) {
                    integralFile << sampleName << " " << hist->Integral() << std::endl;
                }
                inteMCtotal += hist->Integral();
            }
            
            if (histName.find("h_Num_PV") != std::string::npos) {
                integralFile << "MCtotal:  " << inteMCtotal << std::endl;
            }
            
            pad1->cd();
            
            TH1 *ratioHist = nullptr;
            double maxY = 0;
            
            if (mcSum) {
                maxY = GetHistogramMaxWithMargin(mcSum, 50.);
            }
            
            if (dataHistograms.find(histName) != dataHistograms.end()) {
                TH1 *dataHist = dataHistograms[histName].get();
                double dataMax = GetHistogramMaxWithMargin(dataHist, 50.);
                if (dataMax > maxY) {
                    maxY = dataMax;
                }
            }
            stack->Draw("HIST");
            stack->SetMaximum(maxY);
            
            stack->GetXaxis()->SetLabelSize(0);
            stack->GetXaxis()->SetTitleSize(0);
            stack->GetYaxis()->SetTitle("Events");
            stack->GetYaxis()->SetTitleSize(0.06);
            stack->GetYaxis()->SetTitleOffset(1.1);
            stack->GetYaxis()->SetLabelSize(0.05);
            
            if (dataHistograms.find(histName) != dataHistograms.end()) {
                TH1 *dataHist = dataHistograms[histName].get();
                dataHist->SetMarkerStyle(20);
                dataHist->SetMarkerSize(1.2);
                dataHist->SetMarkerColor(kBlack);
                dataHist->SetLineColor(kBlack);
                dataHist->Draw("SAME Ex0");
                legend->AddEntry(dataHist, Form("Data"), "APE");
                
                if (dataHist != nullptr) {
                    if (histName.find("h_Num_PV") != std::string::npos) {
                        integralFile << "Data  " << dataHist->Integral() << std::endl;
                        integralFile << "Frac(Data/MC)  " << dataHist->Integral()/inteMCtotal << std::endl;
                    }
                    
                    ratioHist = (TH1*)dataHist->Clone("ratioHist");
                    ratioHist->SetTitle("");
                    ratioHist->Divide(mcSum);
                    ratioHist->SetMinimum(0.5);
                    ratioHist->SetMaximum(1.5);
                } else {
                    std::cout << "no file in Data : " << histName << std::endl;
                }
                if (histName.find("h_Num_PV") != std::string::npos) {
                    integralFile << std::endl;
                }
            }
            
            legend->Draw();
            CMS_lumi(pad1, "Preliminary", lumiText.c_str());
            if (!channelLabel.empty()) {
                TLatex *chLabel = new TLatex();
                chLabel->SetNDC();
                chLabel->SetTextFont(42);
                chLabel->SetTextSize(0.040);
                chLabel->SetTextAlign(13);
                chLabel->DrawLatex(0.22, 0.74, getChannelLabel(channelLabel).c_str());
            }
            
            if (ratioHist) {
                pad2->cd();
                
                ratioHist->SetStats(0);
                ratioHist->GetYaxis()->SetTitle("Data/MC");
                ratioHist->GetYaxis()->SetTitleSize(0.12);
                ratioHist->GetYaxis()->SetTitleOffset(0.5);
                ratioHist->GetYaxis()->SetLabelSize(0.1);
                ratioHist->GetYaxis()->SetNdivisions(505);
                
                ratioHist->GetXaxis()->SetLabelSize(0.12);
                ratioHist->GetXaxis()->SetTitleSize(0.12);
                ratioHist->GetXaxis()->SetTitleOffset(1.0);
                ratioHist->GetXaxis()->SetTitle(mcSum->GetXaxis()->GetTitle());
                
                ratioHist->Draw("E1P");
                
                TLine *line = new TLine(ratioHist->GetXaxis()->GetXmin(), 1.0, 
                                       ratioHist->GetXaxis()->GetXmax(), 1.0);
                line->SetLineStyle(2);
                line->SetLineColor(kRed);
                line->SetLineWidth(2);
                line->Draw();
            }
            
            savePlotsWithBothScales(canvas, pad1, outputDir, histName, mcSum, maxY);
            printProgress("Plotting", histIdxIndv, nHistsIndv, histName);
            
            canvas.Clear();
            delete ratioHist;
            delete mcSum;
        }
    }
    integralFile.close();
    std::cout << "[Done] Output: Histograms/" << outputDir << "/" << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc < 6 || argc > 9) {
        std::cerr << "Usage: " << argv[0] << " <input_file_list> <color_config_file> <scale_config_file> <hist_config_file> <output_dir> [lumi_text] [category_mode] [channel]" << std::endl;
        std::cerr << "  category_mode: 0 = individual samples (default), 1 = category mode" << std::endl;
        std::cerr << "  channel: MuMu, ElEl, MuEl, Dilepton (optional, for plot label)" << std::endl;
        return 1;
    }

    std::string lumiText    = "13 TeV";
    bool useCategoryMode    = false;
    std::string channelLabel = "";

    if (argc >= 7) lumiText      = argv[6];
    if (argc >= 8) useCategoryMode = (std::stoi(argv[7]) == 1);
    if (argc >= 9) channelLabel  = argv[8];

    StackAndOverlayHistograms(argv[1], argv[2], argv[3], argv[4], argv[5], lumiText, useCategoryMode, channelLabel);
    return 0;
}
