#ifndef FIZMO_AUDIO_OPUS_TABLES_HPP
#define FIZMO_AUDIO_OPUS_TABLES_HPP

#include <cstdint>

namespace fizmo {
namespace audio {
namespace opus_detail {

extern const std::uint8_t k_silk_lbrr2[4];
extern const std::uint8_t k_silk_lbrr3[8];
extern const std::uint8_t k_silk_stereo_stage1[25];
extern const std::uint8_t k_silk_stereo_stage2[3];
extern const std::uint8_t k_silk_stereo_stage3[5];
extern const std::int16_t k_silk_stereo_weights[16];
extern const std::uint8_t k_silk_mid_only[2];
extern const std::uint8_t k_silk_frame_type[2][6];
extern const std::uint8_t k_silk_gain_msb[3][8];
extern const std::uint8_t k_silk_gain_lsb[8];
extern const std::uint8_t k_silk_gain_delta[41];
extern const std::uint8_t k_silk_lsf_stage1[4][32];
extern const std::uint8_t k_silk_lsf_stage2[16][9];
extern const std::uint8_t k_silk_lsf_cb_nb[32][10];
extern const std::uint8_t k_silk_lsf_cb_wb[32][16];
extern const std::uint8_t k_silk_lsf_ext[7];
extern const std::uint8_t k_silk_lsf_pred_nb[2][9];
extern const std::uint8_t k_silk_lsf_pred_wb[2][15];
extern const std::uint8_t k_silk_lsf_pred_sel_nb[32][9];
extern const std::uint8_t k_silk_lsf_pred_sel_wb[32][15];
extern const std::uint8_t k_silk_lsf_cb1_nb[32][10];
extern const std::uint8_t k_silk_lsf_cb1_wb[32][16];
extern const std::int16_t k_silk_lsf_min_nb[11];
extern const std::int16_t k_silk_lsf_min_wb[17];
extern const std::uint8_t k_silk_lsf_interp[5];
extern const std::uint8_t k_silk_lsf_order_nb[10];
extern const std::uint8_t k_silk_lsf_order_wb[16];
extern const std::int16_t k_silk_cos_q12[129];
extern const std::uint8_t k_silk_lag_high[32];
extern const std::uint8_t k_silk_lag_low_nb[4];
extern const std::uint8_t k_silk_lag_low_mb[6];
extern const std::uint8_t k_silk_lag_low_wb[8];
extern const std::uint8_t k_silk_lag_delta[21];
extern const std::uint8_t k_silk_contour_pdf_nb10[3];
extern const std::uint8_t k_silk_contour_pdf_nb20[11];
extern const std::uint8_t k_silk_contour_pdf_wb10[12];
extern const std::uint8_t k_silk_contour_pdf_wb20[34];
extern const std::int8_t k_silk_contour_nb10[3][2];
extern const std::int8_t k_silk_contour_nb20[11][4];
extern const std::int8_t k_silk_contour_wb10[12][2];
extern const std::int8_t k_silk_contour_wb20[34][4];
extern const std::uint8_t k_silk_periodicity[3];
extern const std::uint8_t k_silk_ltp_pdf0[8];
extern const std::uint8_t k_silk_ltp_pdf1[16];
extern const std::uint8_t k_silk_ltp_pdf2[32];
extern const std::int8_t k_silk_ltp_taps0[8][5];
extern const std::int8_t k_silk_ltp_taps1[16][5];
extern const std::int8_t k_silk_ltp_taps2[32][5];
extern const std::uint8_t k_silk_ltp_scale[3];
extern const std::uint8_t k_silk_seed[4];
extern const std::uint8_t k_silk_rate_level[2][9];
extern const std::uint8_t k_silk_pulse_count[11][18];
extern const std::uint8_t k_silk_shell_16[16][17];
extern const std::uint8_t k_silk_shell_8[16][17];
extern const std::uint8_t k_silk_shell_4[16][17];
extern const std::uint8_t k_silk_shell_2[16][17];
extern const std::uint8_t k_silk_lsb[2];
extern const std::uint8_t k_silk_sign[42][2];

} // namespace opus_detail
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_OPUS_TABLES_HPP
