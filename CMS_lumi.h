#ifndef CMS_lumi_h
#define CMS_lumi_h

#include <TLatex.h>
#include <TPad.h>
#include <TLine.h>

// CMS Lumi Text - CMS는 왼쪽 위, Preliminary는 그 아래, 루미노시티는 오른쪽 위에 표시
void CMS_lumi(TPad* pad, const char* extraText = "Preliminary", const char* lumiText = "13 TeV") {
    TLatex* latex = new TLatex();
    latex->SetNDC();
    latex->SetTextAngle(0);
    latex->SetTextColor(kBlack);    
    
    // CMS 로고 위치 - 왼쪽 위
    latex->SetTextFont(61); // bold
    latex->SetTextSize(0.07); // 크기 키움
    latex->DrawLatex(0.2, 0.83, "CMS");
    
    // Preliminary 위치 - CMS 아래에 위치
    latex->SetTextFont(52); // italic
    latex->SetTextSize(0.05); // 크기 키움
    if(extraText) {
        latex->DrawLatex(0.2, 0.78, extraText);
    }
    
    // 루미노시티 텍스트 위치 - 오른쪽 위
    latex->SetTextFont(42); // normal
    latex->SetTextSize(0.05); // 크기 키움
    latex->SetTextAlign(31); // right aligned
    latex->DrawLatex(0.94, 0.94, lumiText);
}

#endif
