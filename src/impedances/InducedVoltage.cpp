
#include <blond/constants.h>
#include <blond/globals.h>
#include <blond/impedances/InducedVoltage.h>
#include <blond/math_functions.h>
#include <blond/fft.h>
#include <blond/utilities.h>


void linear_interp_kick(
    const ftype *__restrict beam_dt,
    ftype *__restrict beam_dE,
    const ftype *__restrict voltage_array,
    const ftype *__restrict bin_centers,
    const int n_slices,
    const int n_macroparticles)
{

    const ftype binFirst = bin_centers[0];
    const ftype binLast = bin_centers[n_slices - 1];
    const ftype inv_bin_width = (n_slices - 1) / (binLast - binFirst);

    #pragma omp parallel for
    for (int i = 0; i < n_macroparticles; ++i) {
        const ftype a = beam_dt[i];
        const int ffbin = static_cast<int>((a - binFirst) * inv_bin_width);
        const ftype voltageKick =
            ((a < binFirst) || (a > binLast))
            ? 0.0
            : voltage_array[ffbin] +
            (a - bin_centers[ffbin]) *
            (voltage_array[ffbin + 1] - voltage_array[ffbin]) *
            inv_bin_width;
        beam_dE[i] += voltageKick;
    }
}

InducedVoltageTime::InducedVoltageTime(std::vector<Intensity *> &WakeSourceList,
                                       time_or_freq TimeOrFreq)
{
    // Induced voltage derived from the sum of
    // several wake fields (time domain).*
    auto Slice = Context::Slice;

    // *Wake sources inputed as a list (eg: list of BBResonators objects)*
    fWakeSourceList = WakeSourceList;

    // *Time array of the wake in [s]*
    fTimeArray = f_vector_t();

    // *Total wake array of all sources in* [:math:`\Omega / s`]
    fTotalWake = f_vector_t();

    // *Induced voltage from the sum of the wake sources in [V]*
    fInducedVoltage = f_vector_t();

    // Pre-processing the wakes
    fTimeArray.resize(Slice->n_slices);
    for (uint i = 0; i < fTimeArray.size(); ++i) {
        fTimeArray[i] = Slice->bin_centers[i] - Slice->bin_centers[0];
    }
    sum_wakes(fTimeArray);

    fCut = fTimeArray.size() + Slice->n_slices - 1;
    fShape = mymath::next_regular(fCut);

    fTimeOrFreq = TimeOrFreq;
}

InducedVoltageTime::~InducedVoltageTime() { fft::destroy_plans(); }

inline void InducedVoltageTime::track()
{
    auto GP = Context::GP;
    auto Beam = Context::Beam;
    auto Slice = Context::Slice;

    // Tracking Method
    f_vector_t v = this->induced_voltage_generation();

    std::transform(v.begin(), v.end(), v.begin(),
                   std::bind1st(std::multiplies<ftype>(), GP->charge));

    linear_interp_kick(Beam->dt.data(), Beam->dE.data(), v.data(),
                       Slice->bin_centers.data(), Slice->n_slices,
                       Beam->n_macroparticles);
}

void InducedVoltageTime::sum_wakes(f_vector_t &TimeArray)
{
    // *Summing all the wake contributions in one total wake.*
    fTotalWake.resize(TimeArray.size());
    std::fill(fTotalWake.begin(), fTotalWake.end(), 0);
    for (Intensity *i : fWakeSourceList) {
        i->wake_calc(TimeArray);
        std::transform(fTotalWake.begin(), fTotalWake.end(), i->fWake.begin(),
                       fTotalWake.begin(), std::plus<ftype>());
    }
}

void InducedVoltageTime::reprocess()
{
    // *Reprocess the wake contributions with respect to the new_slicing.*
    // WARNING As Slice is a global variable,
    // users will have to change this variable and call reprocess()

    auto Slice = Context::Slice;
    fTimeArray.resize(Slice->n_slices);
    for (uint i = 0; i < fTimeArray.size(); ++i) {
        fTimeArray[i] = Slice->bin_centers[i] - Slice->bin_centers[0];
    }
    sum_wakes(fTimeArray);

    fCut = fTimeArray.size() + Slice->n_slices - 1;
    fShape = mymath::next_regular(fCut);
}

f_vector_t InducedVoltageTime::induced_voltage_generation(uint length)
{

    // Method to calculate the induced voltage from wakes with convolution.*
    auto GP = Context::GP;
    auto Beam = Context::Beam;
    auto Slice = Context::Slice;
    f_vector_t inducedVoltage;

    const ftype factor = -GP->charge * constant::e * Beam->intensity
                         / Beam->n_macroparticles;

    if (fTimeOrFreq == freq_domain) {
        f_vector_t in1(Slice->n_slices);
        std::copy(Slice->n_macroparticles.begin(),
                  Slice->n_macroparticles.end(), in1.begin());

        f_vector_t in2 = fTotalWake;

        fft::convolution_with_ffts(in1, in2, inducedVoltage);

        std::transform(inducedVoltage.begin(), inducedVoltage.end(),
                       inducedVoltage.begin(),
                       std::bind1st(std::multiplies<ftype>(), factor));

    } else if (fTimeOrFreq == time_domain) {
        f_vector_t temp(Slice->n_slices);
        std::copy(Slice->n_macroparticles.begin(),
                  Slice->n_macroparticles.end(), temp.begin());

        inducedVoltage.resize(fTotalWake.size() + temp.size() - 1);

        mymath::convolution(fTotalWake.data(), fTotalWake.size(), temp.data(),
                            temp.size(), inducedVoltage.data());

        std::transform(inducedVoltage.begin(), inducedVoltage.end(),
                       inducedVoltage.begin(),
                       std::bind1st(std::multiplies<ftype>(), factor));

    } else {
        dprintf("Error: Only freq_domain or time_domain are allowed\n");
        exit(-1);
    }

    inducedVoltage.resize((uint)Slice->n_slices);
    fInducedVoltage = inducedVoltage;

    if (length > 0)
        inducedVoltage.resize(std::min((uint)Slice->n_slices, length), 0);

    return inducedVoltage;
}

InducedVoltageFreq::InducedVoltageFreq(
    std::vector<Intensity *> &impedanceSourceList, ftype freqResolutionInput,
    freq_res_option_t freq_res_option, uint NTurnsMem,
    bool recalculationImpedance, bool saveIndividualVoltages)
{
    auto Slice = Context::Slice;

    fNTurnsMem = NTurnsMem;

    fImpedanceSourceList = impedanceSourceList;
    fFreqResolutionInput = freqResolutionInput;

    // *Length of one slice.*
    auto timeResolution = (Slice->bin_centers[1] - Slice->bin_centers[0]);
    fRecalculationImpedance = recalculationImpedance;
    fFreqResOption = freq_res_option;

    if (fNTurnsMem == 0) {

        if (fFreqResolutionInput == 0) {
            fNFFTSampling = Slice->n_slices;
        } else {
            int a;
            ftype b = 1 / (fFreqResolutionInput * timeResolution);
            switch (fFreqResOption) {
                case freq_res_option_t::round_option:
                    a = std::round(b);
                    break;
                case freq_res_option_t::ceil_option:
                    a = std::ceil(b);
                    break;
                case freq_res_option_t::floor_option:
                    a = std::floor(b);
                    break;
                default:
                    std::cerr << "The input freq_res_option is not recognized\n";
                    exit(-1);
                    break;
            }
            fNFFTSampling = mymath::next_regular(a);

            if (fNFFTSampling < (uint)Slice->n_slices) {
                std::cerr << "The input frequency resolution step is too big, "
                          "and the whole\n"
                          << "bunch is not sliced... The number of sampling "
                          "points for the\n"
                          << "FFT is corrected in order to sample the whole "
                          "bunch (and\n"
                          << "you might consider changing the input in order "
                          "to have\n"
                          << "a finer resolution\n";
                fNFFTSampling = mymath::next_regular(Slice->n_slices);
            }
        }

        fFreqResolution = 1 / (fNFFTSampling * timeResolution);

        // self.frequency_array = rfftfreq(self.n_fft_sampling,
        // self.slices.bin_centers[1] - self.slices.bin_centers[0])
        fFreqArray = fft::rfftfreq(fNFFTSampling, timeResolution);
        sum_impedances(fFreqArray);

        fSaveIndividualVoltages = saveIndividualVoltages;
        if (fSaveIndividualVoltages) {
            // Do I really need to store the length??
            uint length = fImpedanceSourceList.size();
            fMatrixSaveIndividualImpedances =
                complex_vector_t(length * fFreqArray.size(), 0);
            fMatrixSaveIndividualVoltages =
                f_vector_t(length * Slice->n_slices, 0);
            for (uint i = 0; i < length; ++i) {
                const uint row_width =
                    fImpedanceSourceList[i]->fImpedance.size();
                for (uint j = 0; j < row_width; ++j) {
                    fMatrixSaveIndividualImpedances[i * row_width + j] =
                        fImpedanceSourceList[i]->fImpedance[j];
                }
            }
        }

    } else {
        fNTurnsMem = NTurnsMem;
        fLenArrayMem = (fNTurnsMem + 1) * Slice->n_slices;
        fLenArrayMemExt = (fNTurnsMem + 2) * Slice->n_slices;
        fNPointsFFT = mymath::next_regular(fLenArrayMemExt);
        fFreqArrayMem = fft::rfftfreq(fNPointsFFT, timeResolution);
        fTotalImpedanceMem =
            complex_vector_t(fFreqArrayMem.size(), complex_t(0, 0));

        fTimeArrayMem.reserve((fNTurnsMem + 1) * Slice->n_slices);
        const ftype factor = Slice->edges.back() - Slice->edges.front();

        for (uint i = 0; i < fNTurnsMem + 1; ++i) {
            for (uint j = 0; j < (uint)Slice->n_slices; ++j) {
                fTimeArrayMem.push_back(Slice->bin_centers[j] + factor * i);
            }
        }

        for (const auto &impObj : fImpedanceSourceList) {
            impObj->imped_calc(fFreqArrayMem);
            std::transform(impObj->fImpedance.begin(), impObj->fImpedance.end(),
                           fTotalImpedanceMem.begin(),
                           fTotalImpedanceMem.begin(), std::plus<complex_t>());
        }
    }
}

InducedVoltageFreq::~InducedVoltageFreq() { fft::destroy_plans(); }

void InducedVoltageFreq::track()
{
    // Tracking Method
    auto GP = Context::GP;
    auto Beam = Context::Beam;
    auto Slice = Context::Slice;

    induced_voltage_generation();
    auto v = fInducedVoltage;
    std::transform(v.begin(), v.end(), v.begin(),
                   std::bind1st(std::multiplies<ftype>(), GP->charge));

    linear_interp_kick(Beam->dt.data(), Beam->dE.data(), v.data(),
                       Slice->bin_centers.data(), Slice->n_slices,
                       Beam->n_macroparticles);
}

void InducedVoltageFreq::sum_impedances(f_vector_t &freq_array)
{

    fTotalImpedance.resize(freq_array.size());
    std::fill(fTotalImpedance.begin(), fTotalImpedance.end(), complex_t(0, 0));
    for (auto &i : fImpedanceSourceList) {
        i->imped_calc(freq_array);
        std::transform(fTotalImpedance.begin(), fTotalImpedance.end(),
                       i->fImpedance.begin(), fTotalImpedance.begin(),
                       std::plus<complex_t>());
    }
}

void InducedVoltageFreq::reprocess()
{
    auto Slice = Context::Slice;
    auto timeResolution = (Slice->bin_centers[1] - Slice->bin_centers[0]);
    if (fFreqResolutionInput == 0) {
        fNFFTSampling = Slice->n_slices;
    } else {
        int a;
        ftype b = 1 / (fFreqResolutionInput * timeResolution);
        switch (fFreqResOption) {
            case freq_res_option_t::round_option:
                a = std::round(b);
                break;
            case freq_res_option_t::ceil_option:
                a = std::ceil(b);
                break;
            case freq_res_option_t::floor_option:
                a = std::floor(b);
                break;
            default:
                std::cerr << "The input freq_res_option is not recognized\n";
                exit(-1);
                break;
        }
        fNFFTSampling = mymath::next_regular(a);

        if (fNFFTSampling < (uint)Slice->n_slices) {
            std::cerr
                    << "The input frequency resolution step is too big, and the "
                    "whole\n"
                    << "bunch is not sliced... The number of sampling points for "
                    "the\n"
                    << "FFT is corrected in order to sample the whole bunch (and\n"
                    << "you might consider changing the input in order to have\n"
                    << "a finer resolution\n";
            fNFFTSampling = mymath::next_regular(Slice->n_slices);
        }
    }

    fFreqResolution = 1 / (fNFFTSampling * timeResolution);

    Slice->beam_spectrum_generation(fNFFTSampling, true);
    fFreqArray = Slice->fBeamSpectrumFreq;

    fTotalImpedance.clear();
    sum_impedances(fFreqArray);
}

f_vector_t InducedVoltageFreq::induced_voltage_generation(uint length)
{
    //    Method to calculate the induced voltage from the inverse FFT of the
    //    impedance times the spectrum (fourier convolution).
    auto GP = Context::GP;
    auto Beam = Context::Beam;
    auto Slice = Context::Slice;

    if (fRecalculationImpedance)
        sum_impedances(fFreqArray);

    Slice->beam_spectrum_generation(fNFFTSampling);
    // std::cout << "fNFFTSampling : " << fNFFTSampling << "\n";
    const auto n = fImpedanceSourceList.size();
    const auto factor = -GP->charge * constant::e * Beam->ratio *
                        Slice->fBeamSpectrumFreq[1] * 2 *
                        (Slice->fBeamSpectrum.size() - 1);

    if (fSaveIndividualVoltages) {

        for (uint i = 0; i < n; ++i) {
            f_vector_t res;
            complex_vector_t in(Slice->fBeamSpectrum.size());

            for (uint j = 0; j < in.size(); ++j) {
                in[j] = fMatrixSaveIndividualImpedances[j * n + i] *
                        Slice->fBeamSpectrum[j];
            }

            fft::irfft(in, res, 0, Context::n_threads);

            assert((int)res.size() >= Slice->n_slices);

            res.resize(Slice->n_slices);

            std::transform(res.begin(), res.end(), res.begin(),
                           std::bind1st(std::multiplies<ftype>(), factor));

            for (uint j = 0; j < (uint)Slice->n_slices; ++j) {
                fMatrixSaveIndividualVoltages[j * n + i] = res[j];
            }
        }

        fInducedVoltage.clear();
        fInducedVoltage.resize(Slice->fBeamSpectrum.size());
        for (uint i = 0; i < Slice->fBeamSpectrum.size(); ++i) {
            ftype sum = 0.0;
            for (uint j = 0; j < n; ++j) {
                sum += fMatrixSaveIndividualVoltages[i * n + j];
            }
            fInducedVoltage[i] = sum;
        }

        return f_vector_t();

    } else {
        f_vector_t res;
        complex_vector_t in(Slice->fBeamSpectrum.size());
        for (uint j = 0; j < in.size(); ++j) {
            in[j] = fTotalImpedance[j] * Slice->fBeamSpectrum[j];
        }

        fft::irfft(in, res, 0, Context::n_threads);
        // std::cout << "res size : " << res.size() << std::endl;
        // std::cout << "n_slices : " << Slice->n_slices << std::endl;
        assert((int)res.size() >= Slice->n_slices);

        res.resize((uint)Slice->n_slices);

        std::transform(res.begin(), res.end(), res.begin(),
                       std::bind1st(std::multiplies<ftype>(), factor));

        fInducedVoltage = res;

        if (length > 0) {
            if (length > res.size())
                res.resize(length, 0);
            else
                res.resize(length);
        }

        return res;
    }
}

TotalInducedVoltage::TotalInducedVoltage(
    std::vector<InducedVoltage *> &InducedVoltageList, uint NTurnsMemory,
    f_vector_t RevTimeArray)
{
    fInducedVoltageList = InducedVoltageList;
    fNTurnsMemory = NTurnsMemory;
    fInducedVoltage = f_vector_t();
    fTimeArray = Context::Slice->bin_centers;
}

TotalInducedVoltage::~TotalInducedVoltage() { fft::destroy_plans(); }

void TotalInducedVoltage::track()
{
    auto GP = Context::GP;
    auto Beam = Context::Beam;
    auto Slice = Context::Slice;

    this->induced_voltage_sum();
    auto v = this->fInducedVoltage;

    std::transform(v.begin(), v.end(), v.begin(),
                   std::bind1st(std::multiplies<ftype>(), GP->charge));

    linear_interp_kick(Beam->dt.data(), Beam->dE.data(), v.data(),
                       Slice->bin_centers.data(), Slice->n_slices,
                       Beam->n_macroparticles);
}

void TotalInducedVoltage::track_memory() {}

void TotalInducedVoltage::track_ghosts_particles() {}

void TotalInducedVoltage::reprocess()
{
    for (auto &v : fInducedVoltageList)
        v->reprocess();
}

f_vector_t TotalInducedVoltage::induced_voltage_sum(uint length)
{
    // Method to sum all the induced voltages in one single array.
    f_vector_t tempIndVolt;
    f_vector_t extIndVolt;

    for (auto &v : fInducedVoltageList) {
        auto a = v->induced_voltage_generation(length);

        if (length > 0) {
            extIndVolt.resize(a.size(), 0);
            std::transform(extIndVolt.begin(), extIndVolt.end(), a.begin(),
                           extIndVolt.begin(), std::plus<ftype>());
        }
        tempIndVolt.resize(v->fInducedVoltage.size(), 0);
        std::transform(tempIndVolt.begin(), tempIndVolt.end(),
                       v->fInducedVoltage.begin(), tempIndVolt.begin(),
                       std::plus<ftype>());
    }

    fInducedVoltage = tempIndVolt;
    return extIndVolt;
}
