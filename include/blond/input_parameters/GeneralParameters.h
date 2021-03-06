/*
 * general_parameters.h
 *
 *  Created on: Mar 8, 2016
 *      Author: kiliakis
 */

#ifndef INPUT_PARAMETERS_GENERALPARAMETERS_H_
#define INPUT_PARAMETERS_GENERALPARAMETERS_H_

#include <blond/configuration.h>
#include <blond/utilities.h>
#include <vector>


class API GeneralParameters {

private:
    void eta_generation();
    void _eta0();
    void _eta1();
    void _eta2();

public:
    enum particle_t { proton, electron, user_input, none };

    int n_sections;
    int n_turns;
    int alpha_order;
    particle_t particle, particle_2;
    ftype mass, mass2;
    ftype charge, charge2;
    ftype cumulative_times;
    f_vector_2d_t alpha;
    f_vector_2d_t momentum;
    f_vector_t ring_length;
    ftype ring_circumference;
    ftype ring_radius;
    f_vector_2d_t beta;
    f_vector_2d_t gamma;
    f_vector_2d_t energy;
    f_vector_2d_t kin_energy;
    f_vector_t cycle_time;
    f_vector_t f_rev, omega_rev;
    f_vector_t t_rev;
    f_vector_2d_t eta_0, eta_1, eta_2;

    GeneralParameters(const int n_turns, f_vector_t &ring_length,
                      f_vector_2d_t &alpha, const int alpha_order,
                      f_vector_2d_t &momentum, const particle_t particle,
                      ftype user_mass = 0, ftype user_charge = 0,
                      const particle_t particle2 = none,
                      ftype user_mass_2 = 0, ftype user_charge_2 = 0,
                      const int number_of_sections = 1);

    ~GeneralParameters();
};

#endif /* INPUT_PARAMETERS_GENERALPARAMETERS_H_ */
