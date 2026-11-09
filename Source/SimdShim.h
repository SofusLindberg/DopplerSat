// SimdShim.h (1.5) - de SSE2-funktioner, SatDsp.h bruger, uden SSE2.
//
// SSE2 findes kun paa Intel / AMD. Paa Apple Silicon (Mac, Logic) bruges disse i stedet: samme navne, samme
// regnestykker, een lane ad gangen og i samme raekkefoelge. Da SSE2's double- og float-regning er almindelig
// IEEE-regning, giver det bit for bit samme lyd, saa laenge compileren ikke slaar gange + plus sammen til FMA
// (CMakeLists: -ffp-contract=off). Kan tvinges til paa Windows med DS_FORCE_SIMD_SHIM=1 for at maale netop det.
//
// Typerne ligger i namespace satdsp, saa de ikke stoeder sammen med de rigtige SSE2-typer, hvis en anden header
// (fx JUCE) har inkluderet <emmintrin.h>. Inde i satdsp finder et ukvalificeret navn disse foerst.
#pragma once

namespace satdsp
{

struct alignas (16) __m128d { double v[2]; };   // lane 0 = venstre, lane 1 = hoejre (som _mm_set_pd (hoejre, venstre))
struct alignas (16) __m128  { float  v[4]; };

inline __m128d _mm_set1_pd (double a)              { return { { a, a } }; }
inline __m128d _mm_setzero_pd()                    { return { { 0.0, 0.0 } }; }
inline __m128d _mm_set_pd (double hi, double lo)   { return { { lo, hi } }; }
inline __m128d _mm_load_pd (const double* p)       { return { { p[0], p[1] } }; }
inline void    _mm_store_pd (double* p, __m128d a) { p[0] = a.v[0]; p[1] = a.v[1]; }
inline __m128d _mm_add_pd (__m128d a, __m128d b)   { return { { a.v[0] + b.v[0], a.v[1] + b.v[1] } }; }
inline __m128d _mm_sub_pd (__m128d a, __m128d b)   { return { { a.v[0] - b.v[0], a.v[1] - b.v[1] } }; }
inline __m128d _mm_mul_pd (__m128d a, __m128d b)   { return { { a.v[0] * b.v[0], a.v[1] * b.v[1] } }; }

inline __m128 _mm_loadu_ps (const float* p)        { return { { p[0], p[1], p[2], p[3] } }; }
inline void   _mm_storeu_ps (float* p, __m128 a)   { for (int i = 0; i < 4; ++i) p[i] = a.v[i]; }
inline __m128 _mm_add_ps (__m128 a, __m128 b)      { return { { a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2], a.v[3] + b.v[3] } }; }
inline __m128 _mm_sub_ps (__m128 a, __m128 b)      { return { { a.v[0] - b.v[0], a.v[1] - b.v[1], a.v[2] - b.v[2], a.v[3] - b.v[3] } }; }
inline __m128 _mm_mul_ps (__m128 a, __m128 b)      { return { { a.v[0] * b.v[0], a.v[1] * b.v[1], a.v[2] * b.v[2], a.v[3] * b.v[3] } }; }

} // namespace satdsp
