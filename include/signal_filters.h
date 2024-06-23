#ifndef SIGNAL_FILTERS_H
#define SIGNAL_FILTERS_H



class LowPassFilter {
public:
    LowPassFilter(double sample_rate_hz, double cutoff_frequency_hz);
    double update(double input);

private:
    double alpha_;
    double prev_output_;
};

#endif // SIGNAL_FILTERS_H
