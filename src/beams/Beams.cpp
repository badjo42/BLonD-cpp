/*
 * Beams.cpp
 *
 *  Created on: Mar 22, 2016
 *      Author: kiliakis
 */

#include <blond/beams/Beams.h>
#include <blond/constants.h>
#include <blond/math_functions.h>

Beams::Beams(const uint _n_macroparticles, const long long _intensity)
{

    this->n_macroparticles = _n_macroparticles;
    this->intensity = _intensity;
    this->dt.resize(n_macroparticles);
    this->dE.resize(n_macroparticles);
    this->id = mymath::arange<int>(1, n_macroparticles + 1);
    this->mean_dt = this->mean_dE = 0;
    this->sigma_dt = this->sigma_dE = 0;
    this->ratio = intensity / n_macroparticles;
    this->epsn_rms_l = 0;
    this->n_macroparticles_lost = 0;
}

Beams::~Beams() {}

uint Beams::n_macroparticles_alive()
{

    return n_macroparticles - n_macroparticles_lost;
}

void Beams::statistics()
{
    ftype m_dE, m_dt, s_dE, s_dt;
    m_dt = m_dE = s_dE = s_dt = 0;
    uint n = 0;
    for (uint i = 0; i < n_macroparticles; ++i) {
        if (id[i] != 0) {
            m_dE += dE[i];
            m_dt += dt[i];
            n++;
        }
    }
    mean_dE = m_dE /= n;
    mean_dt = m_dt /= n;
    for (uint i = 0; i < n_macroparticles; ++i) {
        if (id[i] != 0) {
            s_dE += (dE[i] - m_dE) * (dE[i] - m_dE);
            s_dt += (dt[i] - m_dt) * (dt[i] - m_dt);
        }
    }
    sigma_dE = sqrt(s_dE / n);
    sigma_dt = sqrt(s_dt / n);

    epsn_rms_l = constant::pi * sigma_dE * sigma_dt; // in eVs

    // Losses
    n_macroparticles_lost = n_macroparticles - n;
}


void Beams::losses_longitudinal_cut(const ftype dt_min, const ftype dt_max)
{
    #pragma omp parallel for
    for (int i = 0; i < (int)n_macroparticles; i++)
        id[i] = (dt[i] - dt_min) * (dt_max - dt[i]) < 0 ? 0 : id[i];
}

void Beams::losses_energy_cut(const ftype dE_min, const ftype dE_max)
{
    #pragma omp parallel for
    for (int i = 0; i < (int)n_macroparticles; ++i)
        id[i] = (dE[i] - dE_min) * (dE_max - dE[i]) < 0 ? 0 : id[i];
}

/*
void Beams::losses_longitudinal_cut(const ftype* __restrict dt,
                                    const ftype dt_min, const ftype dt_max,
                                    int* __restrict id) {

#pragma omp parallel for
    for (int i = 0; i < (int)n_macroparticles; i++) {
        id[i] = (dt[i] - dt_min) * (dt_max - dt[i]) < 0 ? 0 : id[i];
    }
}

void Beams::losses_energy_cut(const ftype* __restrict dE, const ftype dE_min,
                              const ftype dE_max, int* __restrict id) {
#pragma omp parallel for
    for (int i = 0; i < (int)n_macroparticles; ++i) {
        id[i] = (dE[i] - dE_min) * (dE_max - dE[i]) < 0 ? 0 : id[i];
    }
}
*/
