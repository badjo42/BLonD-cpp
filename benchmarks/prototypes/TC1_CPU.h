#include <blond/globals.h>
#include <blond/math_functions.h>
#include <blond/utilities.h>
#include <valarray>
struct TC1_CPU {
    TC1_CPU() {
        this->acceleration_kick.resize(Context::GP->n_turns + 1);
        // = new ftype[RfP->n_rf * (GP->n_turns)];
        for (uint i = 0; i < Context::RfP->E_increment.size(); ++i) {
            acceleration_kick[i] = -Context::RfP->E_increment[i];
        }
    }

    void track() {
        RingAndRfSection_track();
        Slice_track();
        Context::RfP->counter++;
    }

    void RingAndRfSection_track() {
        kick(Context::RfP->counter);
        drift(Context::RfP->counter + 1);
    }

    void Slice_track() {
        auto Beam = Context::Beam;

        histogram(Beam->dt.data(), Context::Slice->n_macroparticles.data(),
                  Context::Slice->cut_left, Context::Slice->cut_right,
                  Context::Slice->n_slices, Beam->n_macroparticles);
    }

  private:
    f_vector_t acceleration_kick;

    void kick(const uint index) {
        auto RfP = Context::RfP;
        auto Beam = Context::Beam;

        kick(Beam->dt.data(), Beam->dE.data(), RfP->n_rf, RfP->voltage,
             RfP->omega_RF, RfP->phi_RF, Beam->n_macroparticles,
             acceleration_kick[index], index);
    }

    void kick(const ftype* __restrict beam_dt, ftype* __restrict beam_dE,
              const int n_rf, const f_vector_2d_t& __restrict voltage,
              const f_vector_2d_t& __restrict omega_RF,
              const f_vector_2d_t& __restrict phi_RF,
              const int n_macroparticles, ftype acc_kick, const uint index) {

        acc_kick /= static_cast<ftype>(n_rf);
        // KICK
        //#pragma omp parallel for collapse(2)
        for (int j = 0; j < n_rf; ++j) {
            auto current_voltage = voltage[j][index];
            auto current_omega_RF = omega_RF[j][index];
            auto current_phi = phi_RF[j][index];
#pragma omp parallel for
            for (int i = 0; i < n_macroparticles; ++i) {
                // const ftype a = omega_RF[j] * beam_dt[i] + phi_RF[j];
                beam_dE[i] +=
                    current_voltage *
                        mymath::fast_sin(current_omega_RF * beam_dt[i] +
                                         current_phi) +
                    acc_kick;
            }
        }
    }

    inline void drift(const uint index) {
        auto GP = Context::GP;
        auto RfP = Context::RfP;
        auto Beam = Context::Beam;

        drift(Beam->dt.data(), Beam->dE.data(), GP->t_rev[index],
              RfP->length_ratio, GP->alpha_order, RfP->eta_0(index),
              RfP->eta_1(index), RfP->eta_2(index), RfP->beta(index),
              RfP->energy(index), Beam->n_macroparticles);
    }

    inline void drift(ftype* __restrict beam_dt,
                      const ftype* __restrict beam_dE, const ftype T0,
                      const ftype length_ratio, const uint alpha_order,
                      const ftype eta_zero, const ftype eta_one,
                      const ftype eta_two, const ftype beta, const ftype energy,
                      const int n_macroparticles) {

        const ftype T = T0 * length_ratio;

        const ftype T_x_coeff = T * eta_zero / (beta * beta * energy);
#pragma omp parallel for
        for (int i = 0; i < n_macroparticles; i++)
            beam_dt[i] += T_x_coeff * beam_dE[i];
    }

    inline void histogram(const ftype* __restrict input, int* __restrict output,
                          const ftype cut_left, const ftype cut_right,
                          const uint n_slices, const uint n_macroparticles) {

        const int inv_bin_width = n_slices / (cut_right - cut_left);
        const int icut_left = cut_left;
        const int icut_right = cut_right;
        const auto threads = omp_get_num_threads();

        typedef int hist_t;
        auto tile =
            (static_cast<int>(n_macroparticles) + threads - 1) / threads;

        auto catch_size = 256;
        static std::vector<std::valarray<int>> catch_map(
            omp_get_max_threads(), std::valarray<int>(n_slices));
        if (catch_map[0].size() != n_slices) {
            catch_map = std::vector<std::valarray<int>>(
                omp_get_max_threads(), std::valarray<int>(n_slices));
        }

        const int l = n_macroparticles % catch_size;
        const int m = n_macroparticles - catch_size;

#pragma omp parallel for
        for (int i = 0; i < n_macroparticles; i += catch_size) {
            std::valarray<int>& map(catch_map[omp_get_thread_num()]);
            auto max = (i > m) ? l : catch_size;
            auto arr = &input[i];

            for (int j = 0; j < max; ++j) {
                int key = arr[j];
                if (key < icut_left || key > icut_right) {
                    continue;
                }

                map[(key - icut_left) * inv_bin_width]++;
            }
        }

#pragma omp parallel for
        for (int i = 0; i < n_slices; ++i) {
            auto buff = 0;

            for (int j = 0; j < catch_map.size(); ++j) {
                buff += catch_map[j][i];
            }
            output[i] = buff;
        }
    }
};
