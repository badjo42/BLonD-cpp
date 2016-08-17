/*
* Slices.cpp
*
*  Created on: Mar 22, 2016
*      Author: kiliakis
*/

#include <algorithm>
#include <blond/beams/Slices.h>
#include <blond/globals.h>
#include <blond/math_functions.h>
#include <omp.h>

//#include <gsl/gsl_multifit_nlin.h>
//#include <gsl/gsl_vector.h>

Slices::Slices(uint n_slices, int n_sigma, ftype cut_left, ftype cut_right,
               cuts_unit_type cuts_unit, fit_type fit_option,
               bool direct_slicing)
{
    this->n_slices = n_slices;
    this->cut_left = cut_left;
    this->cut_right = cut_right;
    this->cuts_unit = cuts_unit;
    this->fit_option = fit_option;
    this->n_sigma = n_sigma;
    this->n_macroparticles.resize(n_slices, 0);
    this->edges.resize(n_slices + 1, 0.0);
    this->bin_centers.resize(n_slices, 0.0);

    set_cuts();

    if (direct_slicing) track();
}

Slices::~Slices() { fft::destroy_plans(); }

void Slices::set_cuts()
{
    auto Beam = Context::Beam;
    /*
    *Method to set the self.cut_left and self.cut_right properties. This is
    done as a pre-processing if the mode is set to 'const_space', for
    'const_charge' this is calculated each turn.*
    *The frame is defined by :math:`n\sigma_{RMS}` or manually by the user.
    If not, a default frame consisting of taking the whole bunch +5% of the
    maximum distance between two particles in the bunch will be taken
    in each side of the frame.*
    */

    if (cut_left == 0 && cut_right == 0) {
        if (n_sigma == 0) {
            sort_particles();
            cut_left =
                Beam->dt.front() - 0.05 * (Beam->dt.back() - Beam->dt.front());
            cut_right =
                Beam->dt.back() + 0.05 * (Beam->dt.back() - Beam->dt.front());
        } else {
            ftype mean_coords = mymath::mean(Beam->dt.data(), Beam->dt.size());
            ftype sigma_coords = mymath::standard_deviation(
                                     Beam->dt.data(), Beam->dt.size(), mean_coords);
            cut_left = mean_coords - n_sigma * sigma_coords / 2;
            cut_right = mean_coords + n_sigma * sigma_coords / 2;
        }
    } else {
        cut_left = convert_coordinates(cut_left, cuts_unit);
        cut_right = convert_coordinates(cut_right, cuts_unit);
    }

    mymath::linspace(edges.data(), cut_left, cut_right, n_slices + 1);
    for (uint i = 0; i < bin_centers.size(); ++i)
        bin_centers[i] = (edges[i + 1] + edges[i]) / 2;
}

void Slices::sort_particles()
{
    /*
    *Sort the particles with respect to their position.*
    */

    auto Beam = Context::Beam;
    struct API particle {
        ftype dE;
        ftype dt;
        int id;
        bool operator<(const particle &other) const
        {
            return dt < other.dt;
        }
    };

    std::vector<particle> particles(Beam->dE.size());
    for (uint i = 0; i < particles.size(); i++)
        particles[i] = {Beam->dE[i], Beam->dt[i], Beam->id[i]};

    std::sort(particles.begin(), particles.end());

    for (uint i = 0; i < particles.size(); i++) {
        Beam->dE[i] = particles[i].dE;
        Beam->dt[i] = particles[i].dt;
        Beam->id[i] = particles[i].id;
    }
}

inline ftype Slices::convert_coordinates(const ftype cut,
        const cuts_unit_type type)
{
    auto RfP = Context::RfP;
    /*
    *Method to convert a value from one input_unit_type to 's'.*
    */
    if (type == s) {
        return cut;
    } else if (type == rad) {
        return cut / RfP->omega_RF[RfP->idx][RfP->counter];
    } else {
        dprintf("WARNING: We were supposed to have either s or rad\n");
    }
    return 0.0;
}

void Slices::track()
{
    slice_constant_space_histogram();
    if (fit_option == fit_type::gaussian_fit)
        gaussian_fit();
}

inline void Slices::slice_constant_space_histogram()
{
    /*
    *Constant space slicing with the built-in numpy histogram function,
    with a constant frame. This gives the same profile as the
    slice_constant_space method, but no compute statistics possibilities
    (the index of the particles is needed).*
    *This method is faster than the classic slice_constant_space method
    for high number of particles (~1e6).*
    */
    auto Beam = Context::Beam;

    histogram(Beam->dt.data(), n_macroparticles.data(), cut_left, cut_right,
              n_slices, Beam->n_macroparticles);
}

inline void Slices::histogram(const ftype *__restrict input,
                              int *__restrict output, const ftype cut_left,
                              const ftype cut_right, const uint n_slices,
                              const uint n_macroparticles)
{

    const ftype inv_bin_width = n_slices / (cut_right - cut_left);
    // const ftype range = cut_right - cut_left;
    // histogram is faster with ints
    typedef int hist_t;
    hist_t *h;
    #pragma omp parallel
    {
        const auto threads = omp_get_num_threads();
        const auto id = omp_get_thread_num();
        auto tile =
            static_cast<int>((n_macroparticles + threads - 1) / threads);
        auto start = id * tile;
        auto end = std::min(start + tile, (int)n_macroparticles);
        const auto row = id * n_slices;

        #pragma omp single
        h = (hist_t *)calloc(threads * n_slices, sizeof(hist_t));

        const auto h_row = &h[row];

        for (int i = start; i < end; ++i) {
            const ftype a = input[i];
            // const ftype a = (input[i] - cut_left);
            if (a < cut_left || a > cut_right)
                continue;
            const uint ffbin =
                static_cast<uint>((a - cut_left) * inv_bin_width);

            // if (a < 0)
            //    continue;

            // const uint ffbin = static_cast<uint>(a * inv_bin_width);

            // if (ffbin >= n_slices)
            //    continue;

            h_row[ffbin]++;
        }
        #pragma omp barrier

        tile = (n_slices + threads - 1) / threads;
        start = id * tile;
        end = std::min(start + tile, (int)n_slices);

        for (int i = start; i < end; i++)
            output[i] = 0;
        // memset(&output[start], 0, (end-start) * sizeof(ftype));

        for (int i = 0; i < threads; ++i) {
            const int r = i * n_slices;
            for (int j = start; j < end; ++j) {
                output[j] += h[r + j];
            }
        }
    }
    if (h)
        free(h);
}

void Slices::track_cuts()
{
    /*
    *Track the slice frame (limits and slice position) as the mean of the
    bunch moves.
    Requires Beam statistics!
    Method to be refined!*
    */
    auto Beam = Context::Beam;

    ftype delta = Beam->mean_dt - 0.5 * (cut_left + cut_right);
    cut_left += delta;
    cut_right += delta;
    for (uint i = 0; i < n_slices + 1; ++i) {
        edges[i] += delta;
    }
    for (uint i = 0; i < n_slices; ++i) {
        bin_centers[i] += delta;
    }
}

inline void Slices::smooth_histogram(const ftype *__restrict input,
                                     int *__restrict output,
                                     const ftype cut_left,
                                     const ftype cut_right, const uint n_slices,
                                     const uint n_macroparticles)
{

    uint i;
    ftype a;
    ftype fbin;
    ftype ratioffbin;
    ftype ratiofffbin;
    ftype distToCenter;
    uint ffbin = 0;
    uint fffbin = 0;
    const ftype inv_bin_width = n_slices / (cut_right - cut_left);
    const ftype bin_width = (cut_right - cut_left) / n_slices;

    for (i = 0; i < n_slices; i++) {
        output[i] = 0;
    }

    for (i = 0; i < n_macroparticles; i++) {
        a = input[i];
        if ((a < (cut_left + bin_width * 0.5)) ||
                (a > (cut_right - bin_width * 0.5)))
            continue;
        fbin = (a - cut_left) * inv_bin_width;
        ffbin = (uint)(fbin);
        distToCenter = fbin - (ftype)(ffbin);
        if (distToCenter > 0.5)
            fffbin = (uint)(fbin + 1.0);
        ratioffbin = 1.5 - distToCenter;
        ratiofffbin = 1 - ratioffbin;
        if (distToCenter < 0.5)
            fffbin = (uint)(fbin - 1.0);
        ratioffbin = 0.5 - distToCenter;
        ratiofffbin = 1 - ratioffbin;
        output[ffbin] = output[ffbin] + ratioffbin;
        output[fffbin] = output[fffbin] + ratiofffbin;
    }
}

void Slices::slice_constant_space_histogram_smooth()
{
    /*
    At the moment 4x slower than slice_constant_space_histogram but smoother.
    */
    auto Beam = Context::Beam;

    smooth_histogram(Beam->dt.data(), this->n_macroparticles.data(), cut_left,
                     cut_right, n_slices, Beam->n_macroparticles);
}

void Slices::rms()
{

    /*
    * Computation of the RMS bunch length and position from the line density
    (bunch length = 4sigma).*
    */
    auto Beam = Context::Beam;

    f_vector_t lineDenNormalized(n_slices); // = new ftype[n_slices];
    f_vector_t array(n_slices);             // = new ftype[n_slices];

    ftype timeResolution = bin_centers[1] - bin_centers[0];
    ftype trap = mymath::trapezoid(n_macroparticles.data(), timeResolution,
                                   Beam->n_macroparticles);

    for (uint i = 0; i < n_slices; ++i)
        lineDenNormalized[i] = n_macroparticles[i] / trap;

    for (uint i = 0; i < n_slices; ++i)
        array[i] = bin_centers[i] * lineDenNormalized[i];

    bp_rms = mymath::trapezoid(array.data(), timeResolution, n_slices);

    for (uint i = 0; i < n_slices; ++i)
        array[i] = (bin_centers[i] - bp_rms) * (bin_centers[i] - bp_rms) *
                   lineDenNormalized[i];

    ftype temp = mymath::trapezoid(array.data(), timeResolution, n_slices);
    bl_rms = 4 * std::sqrt(temp);
}

void Slices::fwhm(const ftype shift)
{

    /*
    * Computation of the bunch length and position from the FWHM
    assuming Gaussian line density.*
    */
    uint max_i = mymath::max(n_macroparticles.data(), n_slices, 1);
    ftype half_max = shift + 0.5 * (n_macroparticles[max_i] - shift);
    ftype timeResolution = bin_centers[1] - bin_centers[0];

    // First aproximation for the half maximum values

    int i = 0;
    while (n_macroparticles[i] < half_max && i < (int)n_slices)
        i++;
    int taux1 = i;
    i = n_slices - 1;
    while (i >= 0 && n_macroparticles[i] < half_max)
        i--;
    int taux2 = i;

    // dprintf("taux1, taux2 = %d, %d\n", taux1, taux2);
    ftype t1, t2;

    // maybe we could specify what kind of exceptions may occur here
    // numerical (divide by zero) or index out of bounds

    // TODO something weird is happening here
    // Python throws an exception only if you access an element after the end of
    // the array
    // but not if you access element before the start of an array
    // (in that case it takes the last element of the array)
    // Cpp does not throw an exception on eiter occassion
    // The right condition is the following in comments
    if (taux1 > 0 && taux2 < (int)n_slices - 1) {
        // if (taux2 < n_slices - 1) {
        try {
            t1 = bin_centers[taux1] -
                 (n_macroparticles[taux1] - half_max) /
                 (n_macroparticles[taux1] - n_macroparticles[taux1 - 1]) *
                 timeResolution;

            t2 = bin_centers[taux2] +
                 (n_macroparticles[taux2] - half_max) /
                 (n_macroparticles[taux2] - n_macroparticles[taux2 + 1]) *
                 timeResolution;
            bl_fwhm = 4 * (t2 - t1) / cfwhm;
            bp_fwhm = (t1 + t2) / 2;
        } catch (...) {
            bl_fwhm = nan("");
            bp_fwhm = nan("");
        }
    } else {
        // catch (...) {
        bl_fwhm = nan("");
        bp_fwhm = nan("");
    }
}

ftype Slices::fast_fwhm()
{

    /*
    * Computation of the bunch length and position from the FWHM
    assuming Gaussian line density.*
    height = np.max(self.n_macroparticles)
    index = np.where(self.n_macroparticles > height/2.)[0]
    return cfwhm*(Slices.bin_centers[index[-1]] - Slices.bin_centers[index[0]])
    */
    auto Beam = Context::Beam;

    uint max_i =
        mymath::max(n_macroparticles.data(), Beam->n_macroparticles, 1);
    ftype half_max = 0.5 * n_macroparticles[max_i];

    int i = 0;
    while (n_macroparticles[i] < half_max && i < (int)n_slices)
        i++;
    int taux1 = i;
    i = n_slices - 1;
    while (i >= 0 && n_macroparticles[i] < half_max)
        i--;
    int taux2 = i;
    // update bp
    return cfwhm * (bin_centers[taux2] - bin_centers[taux1]);
}

void Slices::fwhm_multibunch() {}

void Slices::beam_spectrum_generation(uint n, bool onlyRFFT)
{

    fBeamSpectrumFreq = fft::rfftfreq(n, bin_centers[1] - bin_centers[0]);

    if (onlyRFFT == false) {
        f_vector_t v(n_macroparticles.begin(), n_macroparticles.end());
        fft::rfft(v, fBeamSpectrum, n, Context::n_threads);
    }
}

void Slices::beam_profile_derivative() {}

void Slices::beam_profile_filter_chebyshev() {}

ftype Slices::gauss(const ftype x, const ftype x0, const ftype sx,
                    const ftype A)
{
    return A * exp(-(x - x0) * (x - x0) / 2.0 / (sx * sx));
}

void Slices::gaussian_fit() {}

/*
struct API  data {
size_t n;
double * y;
};
int gauss(const gsl_vector * x, void *data, gsl_vector * f) {
size_t n = ((struct API  data *) data)->n;
double *y = ((struct API  data *) data)->y;
double A = gsl_vector_get(x, 2);
double x0 = gsl_vector_get(x, 0);
double sx = gsl_vector_get(x, 1);
size_t i;
for (i = 0; i < n; i++) {
// Model Yi = A * exp(-lambda * i) + b
double t = i;
double Yi = A * exp(-(y[i] - x0) * (y[i] - x0) / 2.0 / (sx * sx));
gsl_vector_set(f, i, Yi - y[i]);
}
return GSL_SUCCESS;
}
void Slices::gaussian_fit() {
// *Gaussian fit of the profile, in order to get the bunch length and
// position. Returns fit values in units of s.*
ftype x0, sx, A;
int max_i = mymath::max(n_macroparticles, n_slices, 1);
A = n_macroparticles[max_i];
if (bl_gauss == 0 && bp_gauss == 0) {
x0 = mymath::mean(beam->dt, beam->n_macroparticles);
sx = mymath::standard_deviation(beam->dt, beam->n_macroparticles, x0);
} else {
x0 = bp_gauss;
sx = bl_gauss / 4;
}
const gsl_multifit_fsolver_type *T; //= gsl_multifit_fdfsolver_lmsder;
gsl_multifit_fsolver *s;
int status, info;
size_t i;
const size_t n = beam->n_macroparticles;
const size_t p = 3;
s = gsl_multifit_fsolver_alloc(T, n, p);
//gsl_matrix *J = gsl_matrix_alloc(n, p);
//gsl_matrix *covar = gsl_matrix_alloc(p, p);
ftype y[n], weights[n];
struct API  data d = { n, y };
gsl_multifit_function f;
ftype x_init[3] = { x0, sx, A };
gsl_vector_view x = gsl_vector_view_array(x_init, p);
gsl_multifit_fsolver_set(s, &f, &x.vector);
//  TODO experimental values
status = gsl_multifit_fsolver_driver(s, 20, 100, 1);
#define FIT(i) gsl_vector_get(s->x, i)
ftype x0_new = FIT(0);
ftype sx_new = FIT(1);
ftype A_new = FIT(2);
// TODO use gsl to calculate the gaussian fit
}
*/