#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <THStack.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TColor.h>
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
std::map<std::string, int> loadColorConfig(const std::string &colorConfigFile) {
    std::map<std::string, int> colorMap;
    std::ifstream infile(colorConfigFile);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open color config file." << std::endl;
        return colorMap;
    }

    std::string line;
    while (std::getline(infile, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string sampleName, colorName;
        int colorOffset;
        char plusSign;
        if (!(iss >> sampleName >> colorName >> plusSign >> colorOffset) || plusSign != '+') {
            std::cerr << "Error: Invalid format in color config file: " << line << std::endl;
            continue;
        }
        std::cout << "sampleName : " << sampleName << " colorName : " << colorName << " colorOffset : " << colorOffset << std::endl;
        int color = kBlack;
        if (colorName == "kRed") {
            color = kRed + colorOffset;
        } else if (colorName == "kBlue") {
            color = kBlue + colorOffset;
        } else if (colorName == "kGreen") {
            color = kGreen + colorOffset;
        } else if (colorName == "kMagenta") {
            color = kMagenta + colorOffset;
        } else if (colorName == "kYellow") {
            color = kYellow + colorOffset;
        } else if (colorName == "kOrange") {
            color = kOrange + colorOffset;
        } else if (colorName == "kAzure") {
            color = kAzure + colorOffset;
        } else if (colorName == "kCyan") {
            color = kCyan + colorOffset;
        } else if (colorName == "kWhite") {
            color = kWhite + colorOffset;
        } else if (colorName == "kBlack") {
            color = kBlack + colorOffset;
        }
        std::cout << "color " << color << std::endl;
        colorMap[sampleName] = color;
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

void savePlotsWithBothScales(TCanvas& canvas, TPad* pad1, const std::string& outputDir, 
                           const std::string& histName, TH1* mcSum, double maxY) {
    
    std::string outputPathLog = "Histograms/" + outputDir + "/" + histName + "_Log.pdf";
    std::string outputPathPngLog = "Histograms/" + outputDir + "/" + histName + "_Log.png";
    canvas.SaveAs(outputPathLog.c_str());
    canvas.SaveAs(outputPathPngLog.c_str());
    
    pad1->cd();
    pad1->SetLogy(0);
    
    THStack* stack = (THStack*)pad1->GetPrimitive(histName.c_str());
    if (stack) {
        stack->SetMaximum(maxY);
        stack->SetMinimum(0);
    }
    
    pad1->Modified();
    pad1->Update();
    
    std::string outputPathLinear = "Histograms/" + outputDir + "/" + histName + "_Linear.pdf";
    std::string outputPathPngLinear = "Histograms/" + outputDir + "/" + histName + "_Linear.png";
    canvas.SaveAs(outputPathLinear.c_str());
    canvas.SaveAs(outputPathPngLinear.c_str());
    
    pad1->SetLogy(1);
    pad1->Modified();
    pad1->Update();
}

void StackAndOverlayHistograms(const std::string &inputFileList, const std::string &colorConfigFile, 
                               const std::string &scaleConfigFile, const std::string &histConfigFile,
                               const std::string &outputDir, const std::string &lumiText = "13 TeV",
                               bool useCategoryMode = false) {
    std::cout << "inputFileList: " << inputFileList << std::endl;
    std::cout << "Category Mode: " << (useCategoryMode ? "ON" : "OFF") << std::endl;
    
    setTDRStyle();
    gStyle->SetOptStat(0);

    std::map<std::string, int> colorMap = loadColorConfig(colorConfigFile);
    ScaleMap scaleMap = loadScaleConfig(scaleConfigFile);
    std::map<std::string, HistConfig> histConfigMap = loadHistConfig(histConfigFile);

    std::ifstream fileList(inputFileList);
    if (!fileList.is_open()) {
        std::cerr << "Error: Could not open input file list." << std::endl;
        return;
    }

    std::map<std::string, std::map<std::string, std::unique_ptr<TH1>>> histograms;
    std::map<std::string, std::unique_ptr<TH1>> dataHistograms;

    std::string line;
    while (std::getline(fileList, line)) {
        std::cout << "line : " << line << std::endl;

        TFile inputFile(line.c_str(), "READ");
        if (!inputFile.IsOpen()) {
            std::cerr << "Error: Could not open file " << line << std::endl;
            continue;
        }

        std::string sampleName = line.substr(line.find_last_of('/') + 1);
        sampleName = sampleName.substr(0, sampleName.find_first_of('.'));

        std::cout << "sampleName " << sampleName << std::endl;

        TIter next(inputFile.GetListOfKeys());
        TKey *key;
        while ((key = (TKey *)next())) {

            std::unique_ptr<TObject> obj(key->ReadObj());
            if (obj && obj->InheritsFrom(TH1::Class())) {
                std::string histName = obj->GetName();
                std::unique_ptr<TH1> hist(dynamic_cast<TH1 *>(obj.release()));
                hist->SetDirectory(0);
                
                applyHistConfig(hist.get(), histConfigMap);

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

    std::cout << "outputDir :" << outputDir << std::endl;
    std::cout << Form("mkdir -p Histograms/%s",outputDir.c_str())<< std::endl;
    gSystem->Exec(Form("mkdir -p Histograms/%s",outputDir.c_str()));

    std::string outputFileName = Form("Histograms/%s/Integral.txt",outputDir.c_str());
    std::ofstream integralFile(outputFileName.c_str());
    if (!integralFile.is_open()) {
        std::cerr << "Error: Could not open output file for integrals." << std::endl;
        return;
    }
    if (useCategoryMode) {
        std::cout << "=== Processing in Category Mode ===" << std::endl;
        
        std::map<std::string, std::map<std::string, std::unique_ptr<TH1>>> categoryHistograms;
        
        for (const auto &samplePair : histograms) {
            const std::string &sampleName = samplePair.first;
            std::string category = getCategoryName(sampleName);
            
            std::cout << "Sample: " << sampleName << " -> Category: " << category << std::endl;
            
            for (const auto &histPair : samplePair.second) {
                const std::string &histName = histPair.first;
                TH1* hist = histPair.second.get();
                
                hist->Scale(getSF(scaleMap, sampleName, category, histName));
                
                if (categoryHistograms[category].find(histName) == categoryHistograms[category].end()) {
                    categoryHistograms[category][histName].reset((TH1*)hist->Clone());
                } else {
                    categoryHistograms[category][histName]->Add(hist);
                }
            }
        }
        for (const auto &histPair : categoryHistograms.begin()->second) {
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
            
            auto legend = std::make_unique<TLegend>(0.6, 0.45, 0.93, 0.88);
            legend->SetBorderSize(0);
            legend->SetFillStyle(0);
            legend->SetTextFont(42);
            legend->SetTextSize(0.03);
            legend->SetMargin(0.2);
            
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
                
                if (category == "DY") {
                    hist->SetFillColor(kBlue);
                } else if (category == "Diboson") {
                    hist->SetFillColor(kWhite);
                } else if (category == "WJets") {
                    hist->SetFillColor(kYellow);
                } else if (category == "SingleTop") {
                    hist->SetFillColor(kMagenta);
                } else if (category == "TTV") {
                    hist->SetFillColor(kCyan);
                } else if (category == "TTbar_Signal") {
                    hist->SetFillColor(kRed);
                } else if (category == "TTbar_Others") {
                    hist->SetFillColor(kRed+2);
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
                maxY = GetHistogramMaxWithMargin(mcSum, 1.5);
            }
            
            if (dataHistograms.find(histName) != dataHistograms.end()) {
                TH1 *dataHist = dataHistograms[histName].get();
                double dataMax = GetHistogramMaxWithMargin(dataHist, 1.5);
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
            
            canvas.Clear();
            delete ratioHist;
            delete mcSum;
        }
    }
    else {
        std::cout << "=== Processing in Individual Sample Mode ===" << std::endl;
        
        for (const auto &histPair : histograms.begin()->second) {
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
            
            auto legend = std::make_unique<TLegend>(0.6, 0.45, 0.93, 0.88);
            legend->SetBorderSize(0);
            legend->SetFillStyle(0);
            legend->SetTextFont(42);
            legend->SetTextSize(0.03);
            legend->SetMargin(0.2);
            
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
                
                if (colorMap.find(sampleName) != colorMap.end()) {
                    hist->SetLineColor(kBlack);
                    hist->SetLineWidth(1);
                    hist->SetFillColor(colorMap[sampleName]);
                } else {
                    std::cerr << "Warning: color not found for sample " << sampleName << std::endl;
                }
                
                hist->SetFillStyle(1001);
                
                {
                    std::string indvCategory = getCategoryName(sampleName);
                    hist->Scale(getSF(scaleMap, sampleName, indvCategory, histName));
                }
                
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
                maxY = GetHistogramMaxWithMargin(mcSum, 1.4);
            }
            
            if (dataHistograms.find(histName) != dataHistograms.end()) {
                TH1 *dataHist = dataHistograms[histName].get();
                double dataMax = GetHistogramMaxWithMargin(dataHist, 1.4);
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
            
            canvas.Clear();
            delete ratioHist;
            delete mcSum;
        }
    }
    integralFile.close();
}

int main(int argc, char *argv[]) {
    if (argc < 6 || argc > 8) {
        std::cerr << "Usage: " << argv[0] << " <input_file_list> <color_config_file> <scale_config_file> <hist_config_file> <output_dir> [lumi_text] [category_mode]" << std::endl;
        std::cerr << "  category_mode: 0 = individual samples (default), 1 = category mode" << std::endl;
        return 1;
    }

    std::string lumiText = "13 TeV";
    bool useCategoryMode = false;
    
    if (argc >= 7) {
        lumiText = argv[6];
    }
    
    if (argc == 8) {
        useCategoryMode = (std::stoi(argv[7]) == 1);
    }

    StackAndOverlayHistograms(argv[1], argv[2], argv[3], argv[4], argv[5], lumiText, useCategoryMode);
    return 0;
}
