#pragma once
#include <cstdint>
#include "ymfm.h"
#include "ymfm_opn.h"

struct MyYmfmIntf : public ymfm::ymfm_interface {};

class YM2203Wrap {
public:
  static constexpr uint32_t kOutputs    = ymfm::ym2203::OUTPUTS;
  static constexpr uint32_t kFmOutputs  = ymfm::ym2203::FM_OUTPUTS;
  static constexpr uint32_t kSsgOutputs = ymfm::ym2203::SSG_OUTPUTS;

  explicit YM2203Wrap(uint32_t clock_hz, ymfm::opn_fidelity fidelity = ymfm::OPN_FIDELITY_MIN)
  : clock(clock_hz), chip(intf)
  {
    chip.set_fidelity(fidelity);
    chip.reset();
    native_sr = chip.sample_rate(clock);
  }

  uint32_t sample_rate_native() const { return native_sr; }

  void write_reg(uint8_t reg, uint8_t data) {
    chip.write_address(reg);
    chip.write_data(data);
  }

  // 1サンプル生成：monoを返しつつ、last_outに各出力(FM/SSG)を保持
  int16_t render_one_mono_i16_and_outputs() {
    chip.generate(&last_out, 1);
    int32_t sum = 0;
    for (uint32_t i = 0; i < kOutputs; ++i) sum += last_out.data[i];
    sum /= (int32_t)kOutputs;
    if (sum < -32768) sum = -32768;
    if (sum >  32767) sum =  32767;
    return (int16_t)sum;
  }

  const ymfm::ym2203::output_data& last_outputs() const { return last_out; }

private:
  uint32_t clock;
  uint32_t native_sr{};
  MyYmfmIntf intf;
  ymfm::ym2203 chip;
  ymfm::ym2203::output_data last_out{};
};
