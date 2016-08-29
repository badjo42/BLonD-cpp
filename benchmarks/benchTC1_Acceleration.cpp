#ifdef __MINGW32__
#undef _GLIBCXX_DEBUG
#endif
#include <TC1_CPU.h>
#include <benchmark/benchmark.h>
#include <iostream>

#include "prototypes/TC1_OpenCL.h"
#include <blond/beams/Distributions.h>
#include <blond/math_functions.h>
#include <blond/trackers/Tracker.h>
#include <gtest/gtest.h>

#ifdef NDEBUG
#define PARAMETERS_SPACE(n)                                                    \
    BENCHMARK(n)                                                               \
        ->Args({100, 2000, 10})                                                \
        ->Args({1000, 2000, 10})                                               \
        ->Args({1000, 2000, 10000})                                            \
        ->Args({10000, 10000, 10})                                             \
        ->Args({10000, 10000, 10000})                                          \
        ->Args({10000, 50000, 10})                                             \
        ->Args({10000, 50000, 10000})                                          \
        ->Args({50000, 50000, 10})                                             \
        ->Args({50000, 50000, 10000});
#else
#define PARAMETERS_SPACE(n)                                                    \
    BENCHMARK(n)                                                               \
        ->Args({100, 2000, 10})                                                \
        ->Args({1000, 2000, 10})                                               \
        ->Args({10000, 10000, 10});
#endif

const ftype epsilon = 1e-8;
const std::string params =
#if defined(NDEBUG) && defined(WIN32)
    "./../"
#endif
    TEST_FILES "/TC1_final/TC1_final_params/";

class TestData {

  protected:
    const long long N_b = 1e9;  // Intensity
    const ftype tau_0 = 0.4e-9; // Initial bunch length, 4 sigma [s]

    int N_t; // Number of turns to track
    int N_p; // Macro-particles
    int N_slices;

  public:
    TestData(int N_t = 2000, int N_p = 100, int N_slices = 10000)
        : N_t(N_t), N_p(N_p), N_slices(N_slices) {
        f_vector_2d_t momentumVec(n_sections, f_vector_t(N_t + 1));
        for (auto& v : momentumVec)
            mymath::linspace(v.data(), p_i, p_f, N_t + 1);

        f_vector_2d_t alphaVec(n_sections, f_vector_t(alpha_order + 1, alpha));

        f_vector_t CVec(n_sections, C);

        f_vector_2d_t hVec(n_sections, f_vector_t(N_t + 1, h));

        f_vector_2d_t voltageVec(n_sections, f_vector_t(N_t + 1, V));

        f_vector_2d_t dphiVec(n_sections, f_vector_t(N_t + 1, dphi));

        Context::GP = new GeneralParameters(N_t, CVec, alphaVec, alpha_order,
                                            momentumVec, proton);

        Context::Beam = new Beams(N_p, N_b);

        Context::RfP = new RfParameters(n_sections, hVec, voltageVec, dphiVec);

        longitudinal_bigaussian(tau_0 / 4, 0, -1, false);

        Context::Slice = new Slices(N_slices);
    }

    virtual ~TestData() {
        // Code here will be called immediately after each test
        // (right before the destructor).
        delete Context::GP;
        delete Context::Beam;
        delete Context::RfP;
        delete Context::Slice;
    }

  private:
    // Machine and RF parameters
    const ftype C = 26658.883;       // Machine circumference [m]
    const long long p_i = 450e9;     // Synchronous momentum [eV/c]
    const ftype p_f = 460.005e9;     // Synchronous momentum, final
    const long long h = 35640;       // Harmonic number
    const ftype V = 6e6;             // RF voltage [V]
    const ftype dphi = 0;            // Phase modulation/offset
    const ftype gamma_t = 55.759505; // Transition gamma
    const ftype alpha =
        1.0 / gamma_t / gamma_t; // First order mom. comp. factor
    const int alpha_order = 1;
    const int n_sections = 1;
    // Tracking details
};

static void BM_TC1Acceleration(benchmark::State& state) {
    auto N_p = state.range(0);
    auto N_t = state.range(1);
    auto N_s = state.range(2);
    while (state.KeepRunning()) {
        state.PauseTiming();
        TestData setup(N_t, N_p, N_s);
        auto Beam = Context::Beam;
        Context::n_threads = omp_get_max_threads();
        omp_set_num_threads(Context::n_threads);
        auto long_tracker =
            std::unique_ptr<RingAndRfSection>(new RingAndRfSection());
        state.ResumeTiming();
        for (int i = 0; i < N_t; ++i) {
            long_tracker->track();
            Context::Slice->track();
        }
        state.PauseTiming();

        if (N_t == 2000 && N_p == 100 && N_s == 10) {
            std::vector<ftype> v;
            util::read_vector_from_file(v, params + "dE");
            for (unsigned int i = 0; i < v.size(); ++i) {
                ftype ref_dE = v[i];
                ftype real_dE = Beam->dE[i];
                ASSERT_NEAR(ref_dE, real_dE,
                            epsilon * std::max(fabs(ref_dE), fabs(real_dE)));
            }
            v.clear();
            util::read_vector_from_file(v, params + "dt");
            for (unsigned int i = 0; i < v.size(); ++i) {
                ftype ref_dt = v[i];
                ftype real_dt = Beam->dt[i];
                ASSERT_NEAR(ref_dt, real_dt,
                            epsilon * std::max(fabs(ref_dt), fabs(real_dt)));
            }
        }

        state.ResumeTiming();
    }
}
PARAMETERS_SPACE(BM_TC1Acceleration)

static void BM_TC1AccelerationCPU(benchmark::State& state) {
    auto N_p = state.range(0);
    auto N_t = state.range(1);
    auto N_s = state.range(2);
    while (state.KeepRunning()) {
        state.PauseTiming();
        TestData setup(N_t, N_p, N_s);
        auto Beam = Context::Beam;
        Context::n_threads = omp_get_max_threads();
        omp_set_num_threads(Context::n_threads);
        auto long_tracker =
            std::unique_ptr<RingAndRfSection>(new RingAndRfSection());
        auto TC1_OpenCL_impl = std::unique_ptr<TC1_CPU>(new TC1_CPU());
        state.ResumeTiming();
        for (int i = 0; i < N_t; ++i) {
            TC1_OpenCL_impl->track();
        }
        state.PauseTiming();

        if (N_t == 2000 && N_p == 100 && N_s == 10) {
            std::vector<ftype> v;
            util::read_vector_from_file(v, params + "dE");
            for (unsigned int i = 0; i < v.size(); ++i) {
                ftype ref_dE = v[i];
                ftype real_dE = Beam->dE[i];
                ASSERT_NEAR(ref_dE, real_dE,
                            epsilon * std::max(fabs(ref_dE), fabs(real_dE)));
            }
            v.clear();
            util::read_vector_from_file(v, params + "dt");
            for (unsigned int i = 0; i < v.size(); ++i) {
                ftype ref_dt = v[i];
                ftype real_dt = Beam->dt[i];
                ASSERT_NEAR(ref_dt, real_dt,
                            epsilon * std::max(fabs(ref_dt), fabs(real_dt)));
            }
        }

        state.ResumeTiming();
    }
}
PARAMETERS_SPACE(BM_TC1AccelerationCPU)

#ifdef WITH_OPENCL
static void BM_TC1AccelerationOpenCL(benchmark::State& state) {
    auto N_p = state.range(0);
    auto N_t = state.range(1);
    auto N_s = state.range(2);
    while (state.KeepRunning()) {
        state.PauseTiming();
        TestData setup(N_t, N_p, N_s);
        auto Beam = Context::Beam;
        Context::n_threads = omp_get_max_threads();
        omp_set_num_threads(Context::n_threads);
        auto long_tracker =
            std::unique_ptr<RingAndRfSection>(new RingAndRfSection());
        auto TC1_OpenCL_impl = std::unique_ptr<TC1_OpenCL>(new TC1_OpenCL());
        state.ResumeTiming();
        for (int i = 0; i < N_t; ++i) {
            TC1_OpenCL_impl->track();
        }
        TC1_OpenCL_impl->write_data();
        state.PauseTiming();

        if (N_t == 2000 && N_p == 100 && N_s == 10) {
            std::vector<ftype> v;
            util::read_vector_from_file(v, params + "dE");
            for (unsigned int i = 0; i < v.size(); ++i) {
                ftype ref_dE = v[i];
                ftype real_dE = Beam->dE[i];
                ASSERT_NEAR(ref_dE, real_dE,
                            epsilon * std::max(fabs(ref_dE), fabs(real_dE)));
            }
            v.clear();
            util::read_vector_from_file(v, params + "dt");
            for (unsigned int i = 0; i < v.size(); ++i) {
                ftype ref_dt = v[i];
                ftype real_dt = Beam->dt[i];
                ASSERT_NEAR(ref_dt, real_dt,
                            epsilon * std::max(fabs(ref_dt), fabs(real_dt)));
            }
        }

        state.ResumeTiming();
    }
}
PARAMETERS_SPACE(BM_TC1AccelerationOpenCL)
#endif

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
#ifndef NDEBUG
    std::cin.get();
#endif

    return 0;
}
