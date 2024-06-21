#ifndef TOWERINFOV4_H
#define TOWERINFOV4_H

#include "TowerInfo.h"
#include "TowerInfov1.h"

class TowerInfov4 : public TowerInfo
{
 public:
  TowerInfov4() {}

  ~TowerInfov4() override {}

  void Reset() override;
  void Clear(Option_t* = "") override;

  void set_energy(float _energy) override { energy = _energy; }
  float get_energy() override { return energy; }

  void set_time(short t) override { time = (t * 1000); }
  short get_time() override { return ((float) time)/ 1000; }

  void set_time_float(float t) override { time = t * 1000; }
  float get_time_float() override { return time / 1000.; }

  void set_chi2(float _chi2) override
  {
    float lnChi2;
    if (_chi2 <= 0)
    {
      lnChi2 = 1;
    }
    else
    {
      lnChi2 = std::log(_chi2 + 1) / std::log(1.08);
    }
    if (lnChi2 > 255.0)
    {
      lnChi2 = 255;
    }
    chi2 = static_cast<uint8_t>(std::round(lnChi2));
  }
  float get_chi2() override { return (pow(1.08, (float) chi2) - 1.0); }

  void set_bitfliprecovery(float _recovered) override 
  {
    if (_recovered == 0) 
    {
      recovered = false;
    } 
    else 
    {
      recovered = true;
    }
  }
  float get_bitfliprecovery() override 
  {
    if (recovered == true) 
    {
      return 1;
    } 
    return 0;
  }

  void set_isHot(bool isHot) override { set_status_bit(0, isHot); }
  bool get_isHot() const override { return get_status_bit(0); }

  void set_isBadTime(bool isBadTime) override { set_status_bit(1, isBadTime); }
  bool get_isBadTime() const override { return get_status_bit(1); }

  void set_isBadChi2(bool isBadChi2) override { set_status_bit(2, isBadChi2); }
  bool get_isBadChi2() const override { return get_status_bit(2); }

  void set_isNotInstr(bool isNotInstr) override { set_status_bit(3, isNotInstr); }
  bool get_isNotInstr() const override { return get_status_bit(3); }

  void set_isNoCalib(bool isNoCalib) override { set_status_bit(4, isNoCalib); }
  bool get_isNoCalib() const override { return get_status_bit(4); }

  void set_isZS(bool isZS) override { set_status_bit(5, isZS); }
  bool get_isZS() const override { return get_status_bit(5); }

  bool get_isGood() const override { return !((bool) status); }

  uint8_t get_status() const override { return status; }

  void set_status(uint8_t _status) override { status = _status; }

  void copy_tower(TowerInfo* tower) override;

 private:
  float energy = 0;
  short time = 0;
  uint8_t chi2 = 0;
  uint8_t status = 0;
  bool recovered = false;

  void set_status_bit(int bit, bool value)
  {
    if (bit < 0 || bit > 7)
    {
      return;
    }
    status &= ~((uint8_t) 1 << bit);
    status |= (uint8_t) value << bit;
  }

  bool get_status_bit(int bit) const
  {
    if (bit < 0 || bit > 7)
    {
      return false;  // default behavior
    }
    return (status & ((uint8_t) 1 << bit)) != 0;
  }

  ClassDefOverride(TowerInfov4, 1);
  // Inherit other methods and properties from TowerInfov1
};

#endif
