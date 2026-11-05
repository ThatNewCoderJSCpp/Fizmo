#ifndef FIZMO_AUDIO_AAC_PS_HPP
#define FIZMO_AUDIO_AAC_PS_HPP

#include "aac_internal.hpp"

#include <complex>
#include <memory>

namespace fizmo {
namespace audio {
namespace aac_internal {

class PsDecoder {
public:
    PsDecoder();
    ~PsDecoder();
    PsDecoder(const PsDecoder&) = delete;
    PsDecoder& operator=(const PsDecoder&) = delete;

    void reset();
    int parse(BitReader& br, int bits_available);
    void process(std::complex<float> (*left)[64], std::complex<float> (*right)[64], const std::complex<float> (*ahead)[5], bool new_data);

    struct State;

private:
    std::unique_ptr<State> m_state;
};

} // namespace aac_internal
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_AAC_PS_HPP
