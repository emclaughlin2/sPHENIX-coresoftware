#include "MbdSig.h"
#include "MbdCalib.h"

#include <phool/phool.h>

#include <TF1.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH2.h>
#include <TMath.h>
#include <TPad.h>
#include <TSpectrum.h>
#include <TSpline.h>
#include <TTree.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>

MbdSig::MbdSig(const int chnum, const int nsamp)
  : _ch{chnum}
  , _nsamples{nsamp}
{
  // std::cout << "In MbdSig::MbdSig(" << _ch << "," << _nsamples << ")" << std::endl;
}

void MbdSig::Init()
{
  TString name;

  name = "hrawpulse";
  name += _ch;
  hRawPulse = new TH1F(name, name, _nsamples, -0.5, _nsamples - 0.5);
  name = "hsubpulse";
  name += _ch;
  hSubPulse = new TH1F(name, name, _nsamples, -0.5, _nsamples - 0.5);

  // gRawPulse = new TGraphErrors(_nsamples);
  gRawPulse = new TGraphErrors();
  name = "grawpulse";
  name += _ch;
  gRawPulse->SetName(name);
  // gSubPulse = new TGraphErrors(_nsamples);
  gSubPulse = new TGraphErrors();
  name = "gsubpulse";
  name += _ch;
  gSubPulse->SetName(name);

  hpulse = hRawPulse;  // hpulse,gpulse point to raw by default
  gpulse = gRawPulse;  // we switch to sub for default if ped is applied

  //ped0stats = std::make_unique<MbdRunningStats>(100);  // use the last 100 events for running pedestal
  ped0stats = new MbdRunningStats(100);  // use the last 100 events for running pedestal
  name = "hPed0_";
  name += _ch;
  hPed0 = new TH1F(name, name, 3000, -0.5, 2999.5);
  // hPed0 = new TH1F(name,name,10000,1,0); // automatically determine the range
  name = "hPedEvt_";
  name += _ch;
  hPedEvt = new TH1F(name, name, 3000, -0.5, 2999.5);

  SetTemplateSize(900, 1000, -10., 20.);
  // SetTemplateSize(300,300,0.,15.);

  // Set pedestal function
  ped_fcn = new TF1("ped_fcn","[0]",0,2);
  ped_fcn->SetLineColor(3);

  // Set tail function
  ped_tail = new TF1("ped_tail","[0]+[1]*exp(-[2]*x)",0,2);
  ped_tail->SetLineColor(2);

  // uncomment this to write out waveforms from events that have pileup from prev. crossing
  /*
  name = "mbdsig"; name += _ch; name += ".txt";
  _pileupfile = new ofstream(name);
  */
}

void MbdSig::SetTemplateSize(const Int_t nptsx, const Int_t nptsy, const Double_t begt, const Double_t endt)
{
  template_npointsx = nptsx;
  template_npointsy = nptsy;
  template_begintime = begt;
  template_endtime = endt;

  template_y.resize(template_npointsx);
  template_yrms.resize(template_npointsx);

  Double_t xbinwid = (template_endtime - template_begintime) / (template_npointsx - 1);
  Double_t ybinwid = (1.1 + 0.1) / template_npointsy;  // yscale... should we vary this?
  
  
  delete h2Template;
  
  delete h2Residuals;
  
  TString name = "h2Template";
  name += _ch;
  h2Template = new TH2F(name, name, template_npointsx, template_begintime - (xbinwid / 2.), template_endtime + (xbinwid / 2),
                        template_npointsy, -0.1 + (ybinwid / 2.0), 1.1 + (ybinwid / 2.0));

  name = "h2Residuals";
  name += _ch;
  h2Residuals = new TH2F(name, name, template_npointsx, template_begintime - (xbinwid / 2.), template_endtime + (xbinwid / 2),
                         80, -20, 20);

  /*
  int nbins[] = { template_npointsx, nbinsy };
  Double_t lowrange[] = { template_begintime-xbinwid/2.0, -0.1+ybinwid/2.0 };
  Double_t highrange[] = { template_endtime+xbinwid/2.0, 1.1+ybinwid/2.0 };
  h2Template = new THnSparseF(name,name,2,nbins,lowrange,highrange);
  */
  // h2Template->cd( gDirectory );
}

void MbdSig::SetMinMaxFitTime(const Double_t mintime, const Double_t maxtime)
{
  fit_min_time = mintime;
  fit_max_time = maxtime;
}

MbdSig::~MbdSig()
{
  if ( _pileupfile )
  {
    _pileupfile->close();
  }
  delete hRawPulse;
  delete hSubPulse;
  delete gRawPulse;
  delete gSubPulse;
  delete hPed0;
  delete ped0stats;
  // h2Template->Write();
  delete h2Template;
  delete h2Residuals;
  delete hAmpl;
  delete hTime;
  delete template_fcn;
  delete ped_fcn;
  delete ped_tail;
}

void MbdSig::SetEventPed0PreSamp(const Int_t presample, const Int_t nsamps, const int max_samp)
{
  ped_presamp = presample;
  ped_presamp_nsamps = nsamps;
  if ( ped_presamp_nsamps < 0 && max_samp > 0 )
  {
    ped_presamp_nsamps = max_samp - presample + 1;
  }
  ped_presamp_maxsamp = max_samp; // max of waveform
}

void MbdSig::SetCalib(MbdCalib *m)
{
  _mbdcal = m;
  _pileup_p0 = m->get_pileup(_ch,0);
  _pileup_p1 = m->get_pileup(_ch,1);
  _pileup_p2 = m->get_pileup(_ch,2);
}

// This sets y, and x to sample number (starts at 0)
void MbdSig::SetY(const Float_t* y, const int invert)
{
  if (hRawPulse == nullptr)
  {
    Init();
  }

  hpulse->Reset();
  f_ampl = -9999.;
  f_time = -9999.;

  for (int isamp = 0; isamp < _nsamples; isamp++)
  {
    hRawPulse->SetBinContent(isamp + 1, y[isamp]);
    gRawPulse->SetPoint(isamp, Double_t(isamp), y[isamp]);
  }

  // Apply pedestal
  if (use_ped0 != 0 || minped0samp >= 0 || minped0x != maxped0x || ped_presamp != 0)
  {
    // std::cout << "sub" << std::endl;
 
    int ispileup = 0; // whether pileup event or not (from prev crossing)
                      // note pileup correction not implemented for CalcEventPed0(),
                      // but that is not used

    if (minped0samp >= 0)
    {
      CalcEventPed0(minped0samp, maxped0samp);
    }
    else if (minped0x != maxped0x)
    {
      CalcEventPed0(minped0x, maxped0x);
    }
    else if (ped_presamp != 0)
    {
      ispileup = CalcEventPed0_PreSamp(ped_presamp, ped_presamp_nsamps);
    }

    for (int isamp = 0; isamp < _nsamples; isamp++)
    {
      hSubPulse->SetBinContent(isamp + 1, invert * (y[isamp] - ped0));
      hSubPulse->SetBinError(isamp + 1, ped0rms);
      gSubPulse->SetPoint(isamp, (Double_t) isamp, invert * (y[isamp] - ped0));
      gSubPulse->SetPointError(isamp, 0., ped0rms);
    }

    if ( ispileup==1 && !std::isnan(_pileup_p0) )
    {
      Remove_Pileup();
    }
  }

  _evt_counter++;
}

void MbdSig::SetXY(const Float_t* x, const Float_t* y, const int invert)
{
  //_verbose = 100;
  if (hRawPulse == nullptr)
  {
    Init();
  }

  hRawPulse->Reset();
  hSubPulse->Reset();
  _status = 0;

  f_ampl = -9999.;
  f_time = -9999.;

  // std::cout << "_nsamples " << _nsamples << std::endl;
  // std::cout << "use_ped0 " << use_ped0 << "\t" << ped0 << std::endl;

  for (int isamp = 0; isamp < _nsamples; isamp++)
  {
    // std::cout << "aaa\t" << isamp << "\t" << x[isamp] << "\t" << y[isamp] << std::endl;
    hRawPulse->SetBinContent(isamp + 1, y[isamp]);
    gRawPulse->SetPoint(isamp, x[isamp], y[isamp]);
    gRawPulse->SetPointError(isamp, 0, 4.0);
  }
  if ( _verbose && _ch==9 )
  {
    gRawPulse->Draw("ap");
    gRawPulse->GetHistogram()->SetTitle(gRawPulse->GetName());
    gPad->SetGridy(1);
    PadUpdate();
  }

  if (use_ped0 != 0 || minped0samp >= 0 || minped0x != maxped0x || ped_presamp != 0)
  {
    int ispileup = 0; // whether pileup event or not (from prev crossing)
                      // note pileup correction not implemented for CalcEventPed0(),
                      // but that is not used

    if (minped0samp >= 0)
    {
      CalcEventPed0(minped0samp, maxped0samp);
    }
    else if (minped0x != maxped0x)
    {
      CalcEventPed0(minped0x, maxped0x);
    }
    else if (ped_presamp != 0)
    {
      ispileup = CalcEventPed0_PreSamp(ped_presamp, ped_presamp_nsamps);
    }

    for (int isamp = 0; isamp < _nsamples; isamp++)
    {
      if ( _verbose && isamp==(_nsamples-1) )
      {
        std::cout << "bbb ch " << _ch << "\t" << isamp << "\t" << x[isamp] << "\t" << invert*(y[isamp]-ped0) << std::endl;
      }
      hSubPulse->SetBinContent(isamp + 1, invert * (y[isamp] - ped0));
      hSubPulse->SetBinError(isamp + 1, ped0rms);
      gSubPulse->SetPoint(isamp, x[isamp], invert * (y[isamp] - ped0));
      gSubPulse->SetPointError(isamp, 0., ped0rms);
    }

    if ( ispileup==1 )
    {
      Remove_Pileup();
    }

    if ( _verbose && _ch==9 )
    {
      std::cout << "SetXY: ch " << _ch << std::endl;
      gSubPulse->Print("ALL");
    }
  }

  _evt_counter++;
  _verbose = 0;
}

void MbdSig::Remove_Pileup()
{
  //_verbose = 100;
  _verbose = 0;

  if ( (_ch/8)%2 == 0 )   // time ch
  {
    float offset = _pileup_p0*gSubPulse->GetPointY(0);

    for (int isamp = 0; isamp < _nsamples; isamp++)
    {
      double x = gSubPulse->GetPointX(isamp);
      double y = gSubPulse->GetPointY(isamp);

      hSubPulse->SetBinContent( isamp + 1, y - offset );
      gSubPulse->SetPoint( isamp, x, y - offset );
    }
  }
  else
  {
    if ( fit_pileup == nullptr )
    {
      TString name = "fit_pileup"; name += _ch;
      fit_pileup = new TF1(name,"gaus",-0.1,4.1);
      fit_pileup->SetLineColor(2);
    }

    fit_pileup->SetRange(-0.1,4.1);
    fit_pileup->SetParameters( _pileup_p0*gSubPulse->GetPointY(0), _pileup_p1, _pileup_p2 );
    
    if ( _verbose )
    {
      gSubPulse->Fit( fit_pileup, "R" );
    }
    else
    {
      gSubPulse->Fit( fit_pileup, "RNQ" );
    }

    for (int isamp = 0; isamp < _nsamples; isamp++)
    {

      double bkg = fit_pileup->Eval(isamp);

      double x = gSubPulse->GetPointX(isamp);
      double y = gSubPulse->GetPointY(isamp);

      float newval = static_cast<float>( y - bkg );

      hSubPulse->SetBinContent( isamp + 1, newval );
      gSubPulse->SetPoint( isamp, x, newval );
    }
  }

  if ( _verbose )
  {
    gSubPulse->Draw("ap");
    PadUpdate();
  }

  _verbose = 0;
}

Double_t MbdSig::GetSplineAmpl()
{
  if (gSubPulse == nullptr)
  {
    std::cout << "gsub bad " << (uint64_t) gSubPulse << std::endl;
    return 0.;
  }

  TSpline3 s3("s3", gSubPulse);

  // First find maximum, to rescale
  f_ampl = -999999.;
  double step_size = 0.01;
  // std::cout << "step size " << step_size << std::endl;
  for (double ix = 0; ix < _nsamples; ix += step_size)
  {
    Double_t val = s3.Eval(ix);
    f_ampl = std::max(val, f_ampl);
  }

  return f_ampl;
}

void MbdSig::WritePedHist()
{
  hPed0->Write();
}

void MbdSig::FillPed0(const Int_t sampmin, const Int_t sampmax)
{
  Double_t x;
  Double_t y;
  for (int isamp = sampmin; isamp <= sampmax; isamp++)
  {
    gRawPulse->GetPoint(isamp, x, y);
    // gRawPulse->Print("all");
    hPed0->Fill(y);

    /*
    // chiu taken out
    ped0stats->Push( y );
    ped0 = ped0stats->Mean();
    ped0rms = ped0stats->RMS();
    */

    // std::cout << "ped0 " << _ch << " " << n << "\t" << ped0 << std::endl;
    // std::cout << "ped0 " << _ch << "\t" << ped0 << std::endl;
  }
}

void MbdSig::FillPed0(const Double_t begin, const Double_t end)
{
  Double_t x;
  Double_t y;
  Int_t n = gRawPulse->GetN();
  for (int isamp = 0; isamp < n; isamp++)
  {
    gRawPulse->GetPoint(isamp, x, y);
    if (x >= begin && x <= end)
    {
      hPed0->Fill(y);

      /*
         ped0stats->Push( y );
         ped0 = ped0stats->Mean();
         ped0rms = ped0stats->RMS();
         */

      // std::cout << "ped0 " << _ch << " " << n << "\t" << x << "\t" << y << std::endl;
    }

    // quit if we are past the ped region
    if (x > end)
    {
      break;
    }
  }
}

void MbdSig::SetPed0(const Double_t mean, const Double_t rms)
{
  ped0 = mean;
  ped0rms = rms;
  use_ped0 = 1;
  hpulse = hSubPulse;
  gpulse = gSubPulse;
  // if ( _ch==8 ) std::cout << "_ch " << _ch << " Ped = " << ped0 << std::endl;
}

// Get Event by Event Ped0 if requested
void MbdSig::CalcEventPed0(const Int_t minpedsamp, const Int_t maxpedsamp)
{
  // if (_ch==8) std::cout << "In MbdSig::CalcEventPed0(int,int)" << std::endl;
  hPedEvt->Reset();

  Double_t x;
  Double_t y;
  for (int isamp = minpedsamp; isamp <= maxpedsamp; isamp++)
  {
    gRawPulse->GetPoint(isamp, x, y);

    hPed0->Fill(y);
    hPedEvt->Fill(y);
    // ped0stats->Push( y );
    // if ( _ch==8 ) std::cout << "ped0stats " << isamp << "\t" << y << std::endl;
  }

  // use straight mean for pedestal
  // Could consider using fit to hPed0 to remove outliers
  float mean = hPedEvt->GetMean();
  float rms = hPedEvt->GetRMS();

  SetPed0(mean, rms);
  // if (_ch==8) std::cout << "ped0stats mean, rms " << mean << "\t" << rms << std::endl;
}

// Get Event by Event Ped0 if requested
void MbdSig::CalcEventPed0(const Double_t minpedx, const Double_t maxpedx)
{
  hPedEvt->Reset();

  Double_t x;
  Double_t y;
  Int_t n = gRawPulse->GetN();

  for (int isamp = 0; isamp < n; isamp++)
  {
    gRawPulse->GetPoint(isamp, x, y);

    if (x >= minpedx && x <= maxpedx)
    {
      hPed0->Fill(y);
      hPedEvt->Fill(y);
      // ped0stats->Push( y );
    }
  }

  // use straight mean for pedestal
  // Could consider using fit to hPed0 to remove outliers
  SetPed0(hPedEvt->GetMean(), hPedEvt->GetRMS());
}

// Get Event by Event Ped0, num samples before peak
// presample is number of samples before peak, nsamps is how many samples
// If difficult to get pedestal, use running mean from previous 100 events
// If a prev event pileup is detected, return 1, otherwise, return 0
int MbdSig::CalcEventPed0_PreSamp(const int presample, const int nsamps)
{
  //std::cout << PHWHERE << std::endl;  //chiu
  //_verbose = 100;
  //ped0stats->Clear();

  int status = 0; // assume no pileup

  // Int_t n = gRawPulse->GetN();
  // Int_t max = gRawPulse->GetHistogram()->GetMaximumBin();
  Long64_t max = ped_presamp_maxsamp;

  // actual max from event
  Long64_t actual_max = TMath::LocMax(gRawPulse->GetN(), gRawPulse->GetY());

  if ( ped_presamp_maxsamp == -1 ) // if there is no maxsamp set, use the max found in this event
  {
    max = actual_max;
  }

  Int_t minsamp = max - presample - nsamps + 1;
  Int_t maxsamp = max - presample;
  //std::cout << "CalcEventPed0_PreSamp: ch ctr max " << _ch << "\t" << _evt_counter << "\t" << max << std::endl;

  if (minsamp < 0)
  {
    static int counter = 0;
    if ( counter<1 )
    {
      std::cout << PHWHERE << " ped minsamp " << minsamp << "\t" << max << "\t" << presample << "\t" << nsamps << std::endl;
      counter++;
    }
    minsamp = 0;
  }
  if (maxsamp < 0)
  {
    static int counter = 0;
    if ( counter<1 )
    {
      std::cout << PHWHERE << " ped maxsamp " << maxsamp << "\t" << max << "\t" << presample << std::endl;
      counter++;
    }
    maxsamp = minsamp;
  }

  if ( _verbose )
  {
    std::cout << "CalcEventPed0_Presamp(), ch " << _ch << std::endl;
  }

  double mean = ped0stats->Mean();
  //double rms = ped0stats->RMS();
  double rms = _mbdcal->get_pedrms(_ch);
  if ( std::isnan(rms) )
  {
    // ped calib doesn't exist, set to average
    rms = 5.0;
  }

  ped_fcn->SetRange(minsamp-0.1,maxsamp+0.1);
  ped_fcn->SetParameter(0,1500.);

  if ( gRawPulse->GetN()==0 )//chiu
  {
    std::cout << PHWHERE << " gRawPulse 0" << std::endl;
  }

  if ( _verbose )
  {
    gRawPulse->Fit( ped_fcn, "RQ" );

    double chi2ndf = ped_fcn->GetChisquare()/ped_fcn->GetNDF();
    if ( chi2ndf > 4.0 )
    {
      gRawPulse->Draw("ap");
      ped_fcn->Draw("same");
      PadUpdate();
    }
  }
  else
  {
    //std::cout << PHWHERE << std::endl;
    gRawPulse->Fit( ped_fcn, "RNQ" );

    double chi2ndf = ped_fcn->GetChisquare()/ped_fcn->GetNDF();
    if ( _pileupfile != nullptr && chi2ndf > 4.0 )
    {
      *_pileupfile << "ped " << _ch << " mean " << mean << "\t";
      for ( int i=0; i<gRawPulse->GetN(); i++)
      {
        *_pileupfile << std::setw(6) << gRawPulse->GetPointY(i);
      }
      *_pileupfile << std::endl;
    }
  }

  double chi2 = ped_fcn->GetChisquare();
  double ndf = ped_fcn->GetNDF();

  if ( chi2/ndf < 4.0 )
  {
    mean = ped_fcn->GetParameter(0);

    Double_t x;
    Double_t y;

    for (int isamp = minsamp; isamp <= maxsamp; isamp++)
    {
      gRawPulse->GetPoint(isamp, x, y);

      // exclude outliers
      if ( fabs(y-mean) < 4.0*rms )
      {
        ped0stats->Push( y );
      }

      if ( _verbose )
      {
        std::cout << "CalcEventPed0_PreSamp: ped0stats " << _ch << "\t" << _evt_counter << "\t" 
          << "isamp " << isamp << "\t" << x << "\t" << y << std::endl;
      }
    }
  }
  else
  {
    // ped fit was bad, we have signal contamination in ped region
    // or other thing going on
    status = 1;

    if ( ped0stats->Size() < ped0stats->MaxNum() && !std::isnan(_mbdcal->get_ped(_ch)) ) // use pre-calib for early events
    {
      mean = _mbdcal->get_ped(_ch);
    }
    else  // use running mean, once enough stats are accumulated, or if no pre-calib pedestals
    {
      mean = ped0stats->Mean();

      if ( _verbose )
      {
        gRawPulse->Draw("ap");
        PadUpdate();

        if ( _evt_counter<100 )
        {
          double ped0statsrms = ped0stats->RMS();
          std::cout << "aaa ch " << _ch << "\t" << _evt_counter << "\t" << mean << "\t" << ped0statsrms << std::endl;
        }
        std::string junk;
        std::cin >> junk;
      }
    }

    // use straight mean for pedestal
    // Could consider using fit to hPed0 to remove outliers
    //rms = ped0stats->RMS();
    //Double_t mean = hPed0->GetMean();
    //Double_t rms = hPed0->GetRMS();
  }

  SetPed0(mean, rms);

  if (_verbose > 0 && _evt_counter < 10)
  {
    std::cout << "CalcEventPed0_PreSamp: ped0stats " << _ch << "\t" << _evt_counter << "\t" << mean << "\t" << rms << std::endl;
    std::cout << "CalcEventPed0_PreSamp: ped0stats " << ped0stats->RMS() << std::endl;
  }

  _verbose = 0;

  return status;
}

Double_t MbdSig::LeadingEdge(const Double_t threshold)
{
  // Find first point above threshold
  // We also make sure the next point is above threshold
  // to get rid of a high fluctuation
  int n = gSubPulse->GetN();
  Double_t* x = gSubPulse->GetX();
  Double_t* y = gSubPulse->GetY();

  int sample = -1;
  for (int isamp = 0; isamp < n; isamp++)
  {
    if (y[isamp] > threshold)
    {
      if (isamp == (n - 1) || y[isamp + 1] > threshold)
      {
        sample = isamp;
        break;
      }
    }
  }
  if (sample < 1)
  {
    return -9999.;  // no signal above threshold
  }

  // Linear Interpolation of start time
  Double_t dx = x[sample] - x[sample - 1];
  Double_t dy = y[sample] - y[sample - 1];
  Double_t dt1 = y[sample] - threshold;

  Double_t t0 = x[sample] - (dt1 * (dx / dy));

  return t0;
}

Double_t MbdSig::dCFD(const Double_t fraction_threshold)
{
  // Find first point above threshold
  // We also make sure the next point is above threshold
  // to get rid of a high fluctuation
  int n = gSubPulse->GetN();
  Double_t* x = gSubPulse->GetX();
  Double_t* y = gSubPulse->GetY();

  // Get max amplitude
  Double_t ymax = TMath::MaxElement(n, y);
  if (f_ampl == -9999.)
  {
    f_ampl = ymax;
  }

  Double_t threshold = fraction_threshold * ymax;  // get fraction of amplitude
  // std::cout << "threshold = " << threshold << "\tymax = " << ymax <<std::endl;

  int sample = -1;
  for (int isamp = 0; isamp < n; isamp++)
  {
    if (y[isamp] > threshold)
    {
      if (isamp == (n - 1) || y[isamp + 1] > threshold)
      {
        sample = isamp;
        break;
      }
    }
  }
  if (sample < 1)
  {
    return -9999.;  // no signal above threshold
  }

  // Linear Interpolation of start time
  Double_t dx = x[sample] - x[sample - 1];
  Double_t dy = y[sample] - y[sample - 1];
  Double_t dt1 = y[sample] - threshold;

  Double_t t0 = x[sample] - (dt1 * (dx / dy));

  return t0;
}

Double_t MbdSig::MBDTDC(const Int_t max_samp)
{
  // Get the amplitude of a fixed sample (max_samp) to get time
  // Used in MBD Time Channels
  Double_t* y = gSubPulse->GetY();

  if (y == nullptr)
  {
    std::cout << "ERROR y == 0" << std::endl;
    return std::numeric_limits<Double_t>::quiet_NaN();
  }

  f_time = y[max_samp];

  if ( y[2]>100. )
  {
    f_time = 0.;
  }

  /*
  if ( _ch==128 )
  {
    std::cout << "msig\t" << _ch << "\t" << f_time << "\t" << f_ampl << std::endl;
    gSubPulse->Print("ALL");
  }
  */

  return f_time;
}

Double_t MbdSig::Integral(const Double_t xmin, const Double_t xmax)
{
  Int_t n = gSubPulse->GetN();
  Double_t* x = gSubPulse->GetX();
  Double_t* y = gSubPulse->GetY();

  f_integral = 0.;
  for (int ix = 0; ix < n; ix++)
  {
    if (x[ix] >= xmin && x[ix] <= xmax)
    {
      // Get dx
      Double_t dx = (x[ix + 1] - x[ix - 1]) / 2.0;
      f_integral += (y[ix] * dx);
    }
  }

  return f_integral;
}

void MbdSig::LocMax(Double_t& x_at_max, Double_t& ymax, Double_t xminrange, Double_t xmaxrange)
{
  _verbose = 0;
  if ( _verbose && _ch==250 )
  {
    gSubPulse->Draw("ap");
    gPad->Modified();
    gPad->Update();
  }

  // Find index of maximum peak
  Int_t n = gSubPulse->GetN();
  Double_t* x = gSubPulse->GetX();
  Double_t* y = gSubPulse->GetY();

  // if flipped or equal, we search the whole range
  if (xmaxrange <= xminrange)
  {
    xminrange = -DBL_MAX;
    xmaxrange = DBL_MAX;
  }

  x_at_max = -DBL_MAX;
  ymax = -DBL_MAX;

  for (int i = 0; i < n; i++)
  {
    // Skip if out of range
    if (x[i] < xminrange)
    {
      continue;
    }
    if (x[i] > xmaxrange)
    {
      break;
    }

    if (y[i] > ymax)
    {
      ymax = y[i];
      x_at_max = x[i];
    }
  }

  if ( _verbose )
  {
    std::string junk;
    std::cin >> junk;
    std::cout << "MbdSig::LocMax, " << x_at_max << "\t" << ymax << std::endl;
    _verbose = 0;
  }
}

void MbdSig::LocMin(Double_t& x_at_min, Double_t& ymin, Double_t xminrange, Double_t xmaxrange)
{
  // Find index of minimum peak (for neg signals)
  Int_t n = gSubPulse->GetN();
  Double_t* x = gSubPulse->GetX();
  Double_t* y = gSubPulse->GetY();

  // if flipped or equal, we search the whole range
  if (xmaxrange <= xminrange)
  {
    xminrange = -DBL_MAX;
    xmaxrange = DBL_MAX;
  }

  ymin = DBL_MAX;

  for (int i = 0; i < n; i++)
  {
    // Skip if out of range
    if (x[i] < xminrange)
    {
      continue;
    }
    if (x[i] > xmaxrange)
    {
      break;
    }

    if (y[i] < ymin)
    {
      ymin = y[i];
      x_at_min = x[i];
    }
  }

  // old way of getting locmax
  // int locmax = TMath::LocMin(n,y);
}

void MbdSig::Print()
{
  Double_t x;
  Double_t y;
  std::cout << "CH " << _ch << std::endl;
  for (int isamp = 0; isamp < _nsamples; isamp++)
  {
    gpulse->GetPoint(isamp, x, y);
    std::cout << isamp << "\t" << x << "\t" << y << std::endl;
  }
}

void MbdSig::DrawWaveform()
{
  int orig_verbose = _verbose;
  if ( _verbose <= 5 )
  {
    _verbose = 6;
  }

  gSubPulse->Draw("ap");
  gSubPulse->GetHistogram()->SetTitle(gSubPulse->GetName());
  gPad->SetGridy(1);
  if ( template_fcn!=nullptr ) template_fcn->Draw("same");  // should check if fit was made
  PadUpdate();

  _verbose = orig_verbose;
}

void MbdSig::PadUpdate() const
{
  // Make sure TCanvas is created externally!
  std::cout << PHWHERE << " PadUpdate\t_verbose = " << _verbose << std::endl;
  if ( _verbose>5 )
  {
    gPad->Modified();
    gPad->Update();
    std::cout << _ch << " ? ";
    if ( _verbose>10 )
    {
      std::string junk;
      std::cin >> junk;

      if (junk[0] == 'w' || junk[0] == 's')
      {
        TString name = "ch";
        name += _ch;
        name += ".png";
        gPad->SaveAs(name);
      }
    }
  }
}

Double_t MbdSig::TemplateFcn(const Double_t* x, const Double_t* par)
{
  // par[0] is the amplitude (relative to the spline amplitude)
  // par[1] is the start time (in sample number)
  // x[0] units are in sample number
  Double_t xx = x[0] - par[1];
  Double_t f = 0.;

  int verbose = 0;
  if ( verbose )
  {
    std::cout << PHWHERE << " " << _ch << " x par0 par1 " << x[0] << "\t" << par[0] << "\t" << par[1] << std::endl;
  }


  // When fit is out of limits of good part of spline, ignore fit
  if (xx < template_begintime || xx > template_endtime || std::isnan(xx) )
  {
    TF1::RejectPoint();

    if ( std::isnan(xx) )
    {
      if (verbose > 0)
      {
	std::cout << PHWHERE << " " << _evt_counter << ", " << _ch
		  << " ERROR x par0 par1 " << x[0] << "\t" << par[0] << "\t" << par[1] << std::endl;
	if ( x[0] == 0. )
	{
	  gSubPulse->Print("ALL");
	}
      }
      return 0.;
    }

    if (xx < template_begintime)
    {
      // Double_t x0,y0;
      Double_t y0 = template_y[0];
      return par[0] * y0;
    }
    if (xx > template_endtime)
    {
      // Double_t x0,y0;
      Double_t y0 = template_y[template_npointsx - 1];
      return par[0] * y0;
    }
  }

  // Linear Interpolation of template
  Double_t x0 = 0.;
  Double_t y0 = 0.;
  Double_t x1 = 0.;
  Double_t y1 = 0.;

  // find the index in the vector which is closest to xx
  Double_t step = (template_endtime - template_begintime) / (template_npointsx - 1);
  Double_t index = (xx - template_begintime) / step;
  //std::cout << "xxx " << index << "\t" << xx << "\t" << template_begintime << "\t" << template_endtime << "\t" << step << std::endl;

  int ilow = TMath::FloorNint(index);
  int ihigh = TMath::CeilNint(index);
  if (ilow < 0 || ihigh >= template_npointsx)
  {
    if (verbose > 0)
    {
      std::cout << "ERROR, ilow ihigh " << ilow << "\t" << ihigh << std::endl;
      std::cout << " " << xx << " " << x[0] << " " << par[1] << std::endl;
    }

    if (ilow < 0)
    {
      ilow = 0;
    }
    else if (ihigh >= template_npointsx)
    {
      ihigh = template_npointsx - 1;
    }
  }

  if (ilow == ihigh)
  {
    f = par[0] * template_y[ilow];
  }
  else
  {
    x0 = template_begintime + ilow * step;
    y0 = template_y[ilow];
    x1 = template_begintime + ihigh * step;
    y1 = template_y[ihigh];
    f = par[0] * (y0 + ((y1 - y0) / (x1 - x0)) * (xx - x0));  // linear interpolation
  }

  // reject points with very bad rms in shape
  if (template_yrms[ilow] >= 1.0 || template_yrms[ihigh] >= 1.0)
  {
    TF1::RejectPoint();
    // return f;
  }

  // Reject points where ADC saturates
  int samp_point = static_cast<int>(x[0]);
  Double_t temp_x;
  Double_t temp_y;
  gRawPulse->GetPoint(samp_point, temp_x, temp_y);
  if (temp_y > 16370)
  {
    //if ( _ch==185 ) std::cout << "ADCSATURATED " << _ch << "\t" << samp_point << "\t" << temp_x << "\t" << temp_y << std::endl;
    TF1::RejectPoint();
  }

  //verbose = 0;
  return f;
}

// sampmax>0 means fit to the peak near sampmax
int MbdSig::FitTemplate( const Int_t sampmax )
{
  //std::cout << PHWHERE << std::endl;  //chiu
  //_verbose = 100;	// uncomment to see fits
  //_verbose = 12;        // don't see pedestal fits

  // Check if channel is empty
  if (gSubPulse->GetN() == 0)
  {
    f_ampl = 0.;
    f_time = std::numeric_limits<Float_t>::quiet_NaN();
    std::cout << "ERROR, gSubPulse empty" << std::endl;
    return 1;
  }

  // Determine if channel is saturated
  Double_t *rawsamps = gRawPulse->GetY();
  Int_t nrawsamps = gRawPulse->GetN();
  int nsaturated = 0;
  for (int ipt=0; ipt<nrawsamps; ipt++)
  {
    if ( rawsamps[ipt] > 16370. )
    {
      nsaturated++;
    }
  }
  /*
  if ( nsaturated>2 && _ch==185 )
  {
    _verbose = 12;
  }
  */


  if (_verbose > 0)
  {
    std::cout << "Fitting ch " << _ch << std::endl;
  }

  // Get x and y of maximum
  Double_t x_at_max{-1.};
  Double_t ymax{0.};
  if ( sampmax>=0 )
  {
    gSubPulse->GetPoint(sampmax, x_at_max, ymax);
    if ( nsaturated<=3 )
    {
      x_at_max -= 2.0;
    }
    else
    {
      x_at_max -= 1.5;
      ymax = 16370.+nsaturated*2000.;
    }
  }
  else
  {
    ymax = TMath::MaxElement( gSubPulse->GetN(), gSubPulse->GetY() );
    x_at_max = TMath::LocMax( gSubPulse->GetN(), gSubPulse->GetY() );
  }

  // Threshold cut
  if ( ymax < 20. )
  {
    f_ampl = 0.;
    f_time = std::numeric_limits<Float_t>::quiet_NaN();
    if ( _verbose>20 )
    {
      // for checking pedestal
      std::cout << "skipping, ymax < 20" << std::endl;
      gSubPulse->Draw("ap");
      gSubPulse->GetHistogram()->SetTitle(gSubPulse->GetName());
      gPad->SetGridy(1);
      PadUpdate();
    }

    _verbose = 0;
    return 1;
  }

  template_fcn->SetParameters(ymax, x_at_max);
  // template_fcn->SetParLimits(1, fit_min_time, fit_max_time);
  // template_fcn->SetParLimits(1, 3, 15);
  // template_fcn->SetRange(template_min_xrange,template_max_xrange);
  if ( nsaturated<=3 )
  {
    template_fcn->SetRange(0, _nsamples);
  }
  else
  {
    template_fcn->SetRange(0, sampmax + nsaturated - 0.5);
  }

  if ( gSubPulse->GetN()==0 )//chiu
  {
    std::cout << PHWHERE << " gSubPulse 0" << std::endl;
  }

  if (_verbose == 0)
  {
    //std::cout << PHWHERE << std::endl;
    gSubPulse->Fit(template_fcn, "RNQ");
  }
  else
  {
    std::cout << "doing fit1 " << x_at_max << "\t" << ymax << std::endl;
    gSubPulse->Fit(template_fcn, "R");
    gSubPulse->Draw("ap");
    gSubPulse->GetHistogram()->SetTitle(gSubPulse->GetName());
    gPad->SetGridy(1);
    PadUpdate();
    //std::cout << "doing fit2 " << _verbose << std::endl;
    //std::cout << "doing fit3 " << _verbose << std::endl;
    //gSubPulse->Print("ALL");
  }

  // Get fit parameters
  f_ampl = template_fcn->GetParameter(0);
  f_time = template_fcn->GetParameter(1);
  if ( f_time<0. || f_time>_nsamples )
  {
    f_time = _nsamples*0.5;  // bad fit last time
  }

  // refit with new range to exclude after-pulses
  template_fcn->SetParameters( f_ampl, f_time );
  if ( nsaturated<=3 )
  {
    template_fcn->SetRange( 0., f_time+4.0 );
  }
  else
  {
    template_fcn->SetRange( 0., f_time+nsaturated+0.8 );
  }

  if (_verbose == 0)
  {
    //std::cout << PHWHERE << std::endl;
    int fit_status = gSubPulse->Fit(template_fcn, "RNQ");
    if ( fit_status<0 )
    {
      std::cout << PHWHERE << "\t" << fit_status << std::endl;
      gSubPulse->Print("ALL");
      gSubPulse->Draw("ap");
      gSubPulse->Fit(template_fcn, "R");
      std::cout << "ampl time before refit " << f_ampl << "\t" << f_time << std::endl;
      f_ampl = template_fcn->GetParameter(0);
      f_time = template_fcn->GetParameter(1);
      std::cout << "ampl time after  refit " << f_ampl << "\t" << f_time << std::endl;
      PadUpdate();
      std::string junk;
      std::cin >> junk;
    }
  }
  else
  {
    gSubPulse->Fit(template_fcn, "R");
    //gSubPulse->Print("ALL");
    std::cout << "ampl time before refit " << f_ampl << "\t" << f_time << std::endl;
    f_ampl = template_fcn->GetParameter(0);
    f_time = template_fcn->GetParameter(1);
    std::cout << "ampl time after  refit " << f_ampl << "\t" << f_time << std::endl;
  }

  f_ampl = template_fcn->GetParameter(0);
  f_time = template_fcn->GetParameter(1);

  //if ( f_time<0 || f_time>30 )
  //if ( (_ch==185||_ch==155||_ch==249) && (fabs(f_ampl) > 44000.) )
  //double chi2 = template_fcn->GetChisquare();
  //double ndf = template_fcn->GetNDF();
  //if ( (_ch==185||_ch==155||_ch==249) && (fabs(chi2/ndf) > 100.) && nsaturated > 3)
  if (_verbose > 0 && fabs(f_ampl) > 0.)
  {
    _verbose = 12;
    std::cout << "FitTemplate " << _ch << "\t" << f_ampl << "\t" << f_time << std::endl;
    std::cout << "            " << template_fcn->GetChisquare()/template_fcn->GetNDF() << std::endl;
    gSubPulse->Draw("ap");
    gSubPulse->GetHistogram()->SetTitle(gSubPulse->GetName());
    gPad->SetGridy(1);
    template_fcn->SetLineColor(4);
    template_fcn->Draw("same");
    PadUpdate();
  }

  _verbose = 0;
  return 1;
}

int MbdSig::SetTemplate(const std::vector<float>& shape, const std::vector<float>& sherr)
{
  template_y = shape;
  template_yrms = sherr;

  if (_verbose)
  {
    std::cout << "SHAPE " << _ch << "\t" << template_y.size() << std::endl;
    for (size_t i = 0; i < template_y.size(); i++)
    {
      if (i % 10 == 0)
      {
        std::cout << i << ":\t" << std::endl;
      }
      std::cout << " " << template_y[i];
    }
    std::cout << std::endl;
  }

  if (template_fcn == nullptr)
  {
    TString name = "template_fcn";
    name += _ch;
    // template_fcn = new TF1(name,this,&MbdSig::TemplateFcn,template_min_xrange,template_max_xrange,2,"MbdSig","TemplateFcn");
    // template_fcn = new TF1(name,this,&MbdSig::TemplateFcn,-10,20,2,"MbdSig","TemplateFcn");
    template_fcn = new TF1(name, this, &MbdSig::TemplateFcn, 0, _nsamples, 2, "MbdSig", "TemplateFcn");
    template_fcn->SetParameters(1, 10);
    template_fcn->SetParName(0, "ampl");
    template_fcn->SetParName(1, "time");
    SetTemplateSize(900, 1000, -10., 20.);

    if (_verbose)
    {
      std::cout << "SHAPE " << _ch << std::endl;
      template_fcn->Draw("acp");
      gPad->Modified();
      gPad->Update();
    }
  }

  return 1;
}
