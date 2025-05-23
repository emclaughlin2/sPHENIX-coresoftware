#ifndef TRIGGER_TRIGGERRUNINFO_H
#define TRIGGER_TRIGGERRUNINFO_H

#include <phool/PHObject.h>

#include <cstdint>
#include <iostream>
#include <string>

class TriggerRunInfo : public PHObject
{
 public:
  TriggerRunInfo() = default;
  ///
  virtual ~TriggerRunInfo() override = default;

  void identify(std::ostream& os = std::cout) const override;
  virtual void setTrigger(int, const std::string&, int, int) { return; }
  virtual void setTriggerScalers(int, int, uint64_t) { return; }
  virtual void setTriggerPrescale(int, double) { return; }
  virtual double getPrescaleByName(const std::string&) const { return 0; }
  virtual double getPrescaleByBit(int) const { return 0; }
  virtual int getInitialPrescaleByName(const std::string&) const { return 0; }
  virtual int getInitialPrescaleByBit(int) const { return 0; }

  virtual uint64_t getScalersByName(const std::string&) const { return 0; }
  virtual uint64_t getScalersByBit(int) const { return 0; }
  virtual uint64_t getLiveScalersByName(const std::string&) const { return 0; }
  virtual uint64_t getLiveScalersByBit(int) const { return 0; }
  virtual uint64_t getRawScalersByName(const std::string&) const { return 0; }
  virtual uint64_t getRawScalersByBit(int) const { return 0; }

  virtual uint32_t getTriggerBitByName(const std::string&) const { return 0; }

  virtual std::string getTriggerName(int) const { return "unknown"; }

 private:  // so the ClassDef does not show up with doc++
  ClassDefOverride(TriggerRunInfo, 1);
};

#endif
