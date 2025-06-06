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
        std::istringstream iss(line);
        std::string sampleName, colorName;
        int colorOffset;
        char plusSign;
        if (!(iss >> sampleName >> colorName >> plusSign >> colorOffset) || plusSign != '+') {
            std::cerr << "Error: Invalid format in color config file: " << line << std::endl;
            continue;
        }
        std::cout << "sampleName : " << sampleName << " colorName : " << colorName << " colorOffset : " << colorOffset << std::endl;
        int color = kBlack; // default color
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
        } // Add more color options as needed
        std::cout << "color " << color << std::endl;
        colorMap[sampleName] = color;
    }

    infile.close();
    return colorMap;
}

std::map<std::string, double> loadScaleConfig(const std::string &scaleConfigFile) {
    std::map<std::string, double> scaleMap;
    std::ifstream infile(scaleConfigFile);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open scale config file." << std::endl;
        return scaleMap;
    }

    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string sampleName;
        double scaleValue;

        if (!(iss >> sampleName >> scaleValue)) {
            std::cerr << "Error: Invalid format in scale config file: " << line << std::endl;
            continue;
        }

        std::cout << "sampleName : " << sampleName << " scaleValue : " << scaleValue << std::endl;
        scaleMap[sampleName] = scaleValue;
    }

    infile.close();
    return scaleMap;
}

// New function to load histogram settings
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
        
        // Read the rest of the line for xAxisLabel
        std::string restOfLine;
        std::getline(iss, restOfLine);
        
        // Remove leading whitespace
        restOfLine.erase(0, restOfLine.find_first_not_of(" \t"));
        xAxisLabel = restOfLine;
        
        // Remove trailing comments if present
        size_t commentPos = xAxisLabel.find("//");
        if (commentPos != std::string::npos) {
            xAxisLabel = xAxisLabel.substr(0, commentPos);
            // Trim trailing whitespace
            xAxisLabel.erase(xAxisLabel.find_last_not_of(" \t") + 1);
        }
        
        // Process escape sequences
        std::string processedLabel = "";
        for (size_t i = 0; i < xAxisLabel.length(); ++i) {
            if (xAxisLabel[i] == '\\' && i + 1 < xAxisLabel.length()) {
                if (xAxisLabel[i+1] == '\\') {
                    processedLabel += ' '; // Replace \\ with a space
                    i++; // Skip the next backslash
                } else {
                    // Keep other escape sequences like \eta, \mu, etc.
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

// Function to check if the histogram name matches a pattern
bool matchesPattern(const std::string &histName, const std::string &pattern) {
    return histName.find(pattern) != std::string::npos;
}

// Function to apply settings to the histogram
void applyHistConfig(TH1 *hist, const std::map<std::string, HistConfig> &histConfigMap) {
    if (!hist) return;
    
    std::string histName = hist->GetName();
    
    for (const auto &configPair : histConfigMap) {
        const std::string &pattern = configPair.first;
        const HistConfig &config = configPair.second;
        
        if (matchesPattern(histName, pattern)) {
            std::cout << "Applying config to " << histName << ": Rebin=" << config.rebinFactor 
                      << ", XAxisLabel=" << config.xAxisLabel << std::endl;
            
            // Apply rebinning
            if (config.rebinFactor > 1) {
                hist->Rebin(config.rebinFactor);
            }
            
            // Apply X-axis label
            if (!config.xAxisLabel.empty()) {
                hist->GetXaxis()->SetTitle(config.xAxisLabel.c_str());
            }
            
            // Apply only the first matching pattern and exit
            break;
        }
    }
}

// Function to add margin to histogram max value
double GetHistogramMaxWithMargin(TH1* hist, double marginFactor = 1.2) {
    if (!hist) return 0;
    double maxVal = hist->GetMaximum();
    return maxVal * marginFactor;  // Add 20% margin
}

void StackAndOverlayHistograms(const std::string &inputFileList, const std::string &colorConfigFile, 
                               const std::string &scaleConfigFile, const std::string &histConfigFile,
                               const std::string &outputDir, const std::string &lumiText = "13 TeV") {
    std::cout << "inputFileList: " << inputFileList << std::endl;
    
    // Apply CMS TDR Style
    setTDRStyle();
    gStyle->SetOptStat(0);

    // Load color configuration
    std::map<std::string, int> colorMap = loadColorConfig(colorConfigFile);

    // Load scale configuration
    std::map<std::string, double> scaleMap = loadScaleConfig(scaleConfigFile);

    // Load histogram configuration
    std::map<std::string, HistConfig> histConfigMap = loadHistConfig(histConfigFile);

    // Open the input file list
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
                hist->SetDirectory(0);  // Detach from file
                
                // Apply histogram settings (Rebinning and X-axis labeling here)
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

    // Create output directory
    std::cout << "outputDir :" << outputDir << std::endl;
    std::cout << Form("mkdir -p Histograms/%s",outputDir.c_str())<< std::endl;
    gSystem->Exec(Form("mkdir -p Histograms/%s",outputDir.c_str()));

    std::string outputFileName = Form("Histograms/%s/Integral.txt",outputDir.c_str());
    std::ofstream integralFile(outputFileName.c_str());
    if (!integralFile.is_open()) {
        std::cerr << "Error: Could not open output file for integrals." << std::endl;
        return;
    }

    for (const auto &histPair : histograms.begin()->second) {
        // Modify canvas creation - set up for pad splitting
        TCanvas canvas("canvas", "Histogram Stacks", 1200, 1200); // Adjust to larger height
        canvas.cd();
        
        // Split into two pads (top: histogram, bottom: ratio)
        // Set top pad to 70% and bottom pad to 30%
        TPad *pad1 = new TPad("pad1", "pad1", 0, 0.3, 1, 1.0);
        pad1->SetBottomMargin(0.02); // Reduce bottom margin of top pad
        pad1->SetLeftMargin(0.16);
        pad1->SetRightMargin(0.05);
        pad1->SetTopMargin(0.1);  // Adjust top margin
        //pad1->SetLogy(1);  // Set log scale for Y axis
        pad1->Draw();
        
        TPad *pad2 = new TPad("pad2", "pad2", 0, 0.0, 1, 0.3);
        pad2->SetTopMargin(0.03); // Reduce top margin of bottom pad
        pad2->SetBottomMargin(0.35); // Bottom margin of bottom pad (for X-axis labels)
        pad2->SetLeftMargin(0.16);
        pad2->SetRightMargin(0.05);
        pad2->Draw();
        
        const std::string &histName = histPair.first;
        auto stack = std::make_unique<THStack>(histName.c_str(), "");  // Leave title blank for CMS style
        
        // Adjust legend size and position - widen to avoid overlap
        auto legend = std::make_unique<TLegend>(0.6, 0.45, 0.93, 0.88);
        legend->SetBorderSize(0);
        legend->SetFillStyle(0);
        legend->SetTextFont(42);
        legend->SetTextSize(0.03); // Reduce font size
        legend->SetMargin(0.2); // Increase left margin
        
        //if (histNamei)
        if (histName.find("h_Num_PV") != std::string::npos) {
            integralFile << histName << std::endl;
            integralFile << std::endl;
        }
        //integralFile << histName << std::endl;
        //integralFile << std::endl;
        
        // Clone to compute total MC histogram
        TH1 *mcSum = nullptr;
        
        double inteMCtotal = 0;
        
        // Determine MC sample order first (for stacking in reverse)
        std::vector<std::string> sampleOrder;
        for (const auto &samplePair : histograms) {
            sampleOrder.push_back(samplePair.first);
        }
        // Process in reverse order (stacking bottom to top)
        std::reverse(sampleOrder.begin(), sampleOrder.end());
        
        for (const auto &sampleName : sampleOrder) {
            auto &samplePair = histograms[sampleName];

            // Check if histName exists in samplePair (the inner map)
            auto histIt = samplePair.find(histName);
            if (histIt == samplePair.end()) {
                std::cerr << "Warning: histogram " << histName << " not found for sample " << sampleName << std::endl;
                continue;
            }
            TH1 *hist = histIt->second.get();
            
            // Create MC sum histogram (for ratio computation)
            if (!mcSum) {
                mcSum = (TH1*)hist->Clone("mcSum");
                mcSum->Reset();
            }
            
            // Check and set color if sampleName exists in colorMap
            if (colorMap.find(sampleName) != colorMap.end()) {
                hist->SetLineColor(kBlack); // Use black border
                hist->SetLineWidth(1);
                hist->SetFillColor(colorMap[sampleName]);
            } else {
                std::cerr << "Warning: color not found for sample " << sampleName << std::endl;
            }
            
            hist->SetFillStyle(1001);
            
            // Check and scale if sampleName exists in scaleMap
            if (scaleMap.find(sampleName) != scaleMap.end()) {
                hist->Scale(scaleMap[sampleName]);
            } else {
                std::cerr << "Warning: scale not found for sample " << sampleName << std::endl;
            }
            
            // Add to MC sum
            mcSum->Add(hist);
            
            stack->Add(hist);
            // Change legend entry format - align decimal spacing
            legend->AddEntry(hist, Form("%s (%.1f)", sampleName.c_str(), hist->Integral()), "f");
             if (histName.find("h_Num_PV") != std::string::npos) {integralFile << sampleName << " " << hist->Integral() << std::endl;}
            inteMCtotal += hist->Integral();
        }
        
         if (histName.find("h_Num_PV") != std::string::npos){integralFile << "MCtotal:  " << inteMCtotal << std::endl;}
        
        // Draw histogram in top pad
        pad1->cd();
        
        // If data exists
        TH1 *ratioHist = nullptr;
        double maxY = 0;
        
        // Compute maximum value from MC stack
        if (mcSum) {
            maxY = GetHistogramMaxWithMargin(mcSum, 1.2);  // Add 20% margin
        }
        
        // If data histogram exists, compare max value too
        if (dataHistograms.find(histName) != dataHistograms.end()) {
            TH1 *dataHist = dataHistograms[histName].get();
            double dataMax = GetHistogramMaxWithMargin(dataHist, 1.2);
            if (dataMax > maxY) {
                maxY = dataMax;
            }
        }
        
        // Draw stack and set Y-axis range
        stack->Draw("HIST");
        stack->SetMaximum(maxY);  // Set maximum value
        
        // Hide X-axis title (shown in bottom pad)
        stack->GetXaxis()->SetLabelSize(0);
        stack->GetXaxis()->SetTitleSize(0);
        stack->GetYaxis()->SetTitle("Events");
        stack->GetYaxis()->SetTitleSize(0.06);
        stack->GetYaxis()->SetTitleOffset(1.1);
        stack->GetYaxis()->SetLabelSize(0.05);
        
        // If data exists 그리기
        if (dataHistograms.find(histName) != dataHistograms.end()) {
            TH1 *dataHist = dataHistograms[histName].get();
            dataHist->SetMarkerStyle(20);
            dataHist->SetMarkerSize(1.0);
            dataHist->SetMarkerColor(kBlack);
            dataHist->SetLineColor(kBlack);
            dataHist->Draw("SAME E1P");
            legend->AddEntry(dataHist, Form("Data (%.0f)", dataHist->Integral()), "lep");
            
            if (dataHist != nullptr) {
                if (histName.find("h_Num_PV") != std::string::npos){
                    integralFile << "Data  " << dataHist->Integral() << std::endl;
                    integralFile << "Frac(MC/Data)  " << inteMCtotal/dataHist->Integral() << std::endl;
                }
                
                // Create Data/MC ratio histogram
                ratioHist = (TH1*)dataHist->Clone("ratioHist");
                ratioHist->SetTitle("");
                ratioHist->Divide(mcSum);
                
                // Set Y-axis range for ratio histogram
                ratioHist->SetMinimum(0.5);  // Minimum of ratio
                ratioHist->SetMaximum(1.5);  // Maximum of ratio
            } else {
                std::cout << "no file in Data : " << histName << std::endl;
            }
            if (histName.find("h_Num_PV") != std::string::npos){ integralFile << std::endl;}
        }
        
        legend->Draw();
        
        // Display CMS logo and text (inside pad)
        CMS_lumi(pad1, "Preliminary", lumiText.c_str());
        
        // Draw ratio in bottom pad
        if (ratioHist) {
            pad2->cd();
            
            // Set ratio histogram style
            ratioHist->SetStats(0);
            ratioHist->GetYaxis()->SetTitle("Data/MC");
            ratioHist->GetYaxis()->SetTitleSize(0.12);
            ratioHist->GetYaxis()->SetTitleOffset(0.5);
            ratioHist->GetYaxis()->SetLabelSize(0.1);
            ratioHist->GetYaxis()->SetNdivisions(505);
            
            // Set X-axis label
            ratioHist->GetXaxis()->SetLabelSize(0.12);
            ratioHist->GetXaxis()->SetTitleSize(0.12);
            ratioHist->GetXaxis()->SetTitleOffset(1.0);
            ratioHist->GetXaxis()->SetTitle(mcSum->GetXaxis()->GetTitle());
            
            // Draw ratio histogram
            ratioHist->Draw("E1P");
            
            // Draw baseline at ratio = 1.0
            TLine *line = new TLine(ratioHist->GetXaxis()->GetXmin(), 1.0, 
                                   ratioHist->GetXaxis()->GetXmax(), 1.0);
            line->SetLineStyle(2); // Dashed line
            line->SetLineColor(kRed);
            line->SetLineWidth(2);
            line->Draw();
        }
        
        // Save file
        //std::string outputPath = "Histograms/"+outputDir+ "/" + histName + "_Log.pdf";
        std::string outputPath = "Histograms/"+outputDir+ "/" + histName + ".pdf";
        canvas.SaveAs(outputPath.c_str());
        //std::string outputPathPng = "Histograms/"+outputDir+ "/" + histName + "_Log.png";
        std::string outputPathPng = "Histograms/"+outputDir+ "/" + histName + ".png";
        canvas.SaveAs(outputPathPng.c_str());
        canvas.Clear();
        
        // Clean up memory
        delete ratioHist;
        delete mcSum;
    }
    integralFile.close();
}

int main(int argc, char *argv[]) {
    if (argc < 6 || argc > 7) {
        std::cerr << "Usage: " << argv[0] << " <input_file_list> <color_config_file> <scale_config_file> <hist_config_file> <output_dir> [lumi_text]" << std::endl;
        return 1;
    }

    std::string lumiText = "13 TeV";
    if (argc == 7) {
        lumiText = argv[6];
    }

    StackAndOverlayHistograms(argv[1], argv[2], argv[3], argv[4], argv[5], lumiText);
    return 0;
}
