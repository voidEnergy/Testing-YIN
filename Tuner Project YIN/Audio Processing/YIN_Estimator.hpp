#ifndef YIN_ESTIMATOR_HPP
#define YIN_ESTIMATOR_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>
using namespace std;

#define db double
typedef vector<double> vdb;

struct YINResult{
    db tau;
    db quality;  // value of d' function at that tau
};

class YINEstimator{
    private:
    db threshold;
    int sampleRate;
    int winsize;
    int maxlag;
    vdb d;

    // Buffering recent estimates to find the best local one
    // Stores the refined answer(refinedTau) and its confidence score(quality) in the YIN Result
    // The vector resultHistory keeps a log of the last 5 frames(result of the set of 3000 samples)
    vector <YINResult> resultHistory;               
    const int historySize = 5; 
    
    public:
        YINEstimator(int sr, int winSize, db thold = 0.15) : threshold(thold), sampleRate(sr), winsize(winSize) {
            maxlag = sr / 40;
            // sets the minimum frequency of receiving sound as 40Hz
            d.resize(maxlag, 0.0);
        }

        // Entry point of the function
        // Takes in buffer sliding window of 3000 audio samples(built in main.cpp)
        // return value is defined as a double(the final frequency in Hertz)
        db getPitch(const vdb &buffer) {
            // Safety check for YIN algo requiring a min amount of audio to do its math
            // If buffer is too small, return -1.0(i.e a valid pitch isn't heard)
            if (buffer.size() < (size_t)(winsize + maxlag)) return -1.0;

            // 1. SILENCE GATE (RMS)
            // Check whether there exists any sound worth analyzing
            db sum = 0;
            for (db sample : buffer) sum += sample * sample;
            db rms = sqrt(sum / buffer.size());

            // 2. YIN MATH
            // Main YIN Steps
            difference(buffer);
            CMNDF(); 

            // 3. CANDIDATE SEARCH
            // Searches for the first significant dip that drops below the thold in the CMNDF array
            int tauEstimate = absThreshold();
            // If a dip was found(not -1), the exact depth of the dip is taken from array d and labelled to quality score
            // If failed to find a dip, return 1.0(worst possible value of CMNDF)
            db quality = (tauEstimate != -1) ? d[tauEstimate] : 1.0;
            // Quality = Exact depth of the dip found in the CMNDF

            // 4. THROTTLED DEBUG DISPLAY
            // Since the function runs hundreds of times per second,
            // printing every single frame would cause terminal window to flicker and slow down the CPU
            // Hence we print the values once for every 20 frames
            static int debugCounter = 0;
            if (++debugCounter % 20 == 0) {
                printf("\r[DEBUG] RMS: %.4f | Quality: %.4f | Tau: %d    ", rms, quality, tauEstimate);
                fflush(stdout);
            }

            // 5. THE GATEKEEPERS
            // Prevents wastage of CPU cycles on  irrelevant data
            // RMS < 0.005 => Volume is nearly silent
            // tauEstimate == -1 => The thold scan could not find a funamental dip
            // quality > 0.5 => If the dip wasn't just deep enough, abort
            if (rms < 0.005) return -1.0;      
            if (tauEstimate == -1) return -1.0; 
            if (quality > 0.5) return -1.0;    

            // 6. REFINEMENT
            db refinedTau = parabolicInterpol(tauEstimate);
            // log the value of the tau found into the log-book
            updateHistory({refinedTau, quality});  
            // Step - 7       
            db bestTau = getBestLocalTau();
            db frequency = (db)sampleRate / bestTau;
            cout << "\r[DEBUG] RMS: " << fixed << setprecision(4) << rms 
                << " | Quality: " << quality 
                << " | Tau: " << bestTau 
                << " | Freq: " << setprecision(2) << frequency << " Hz    " << flush;

            return frequency;
            // return (db)sampleRate / bestTau;
        }
    
    private :
        // Step - 3(Using diff instead of ACF)
        void difference(const vdb &buffer){                 // finds the square of diff fxn for various values of tau
            fill(d.begin(), d.end(), 0.0);
            for (int tau = 1; tau < maxlag; tau++){         // taking different values of tau
                for(int j = 0; j < winsize; j++){
                    db diff = buffer[j] - buffer[j + tau];  // takes the diff. i.e. signal - shifted signal
                    d[tau] += diff * diff;                  // squares the diff
                }
            }
        }
        // Step - 4 (Cumulative Mean Normalized Difference function)
        void CMNDF(){
            d[0] = 1.0;
            db runningSum = 0.0;
            for (int tau = 1; tau < maxlag; tau++){
                runningSum += d[tau];
                d[tau] /= (runningSum / tau);
            }
        }
        // Step - 5 (Absolute Threshold)
        int absThreshold(){
            for (int tau = 2; tau < maxlag; tau++){
                if (d[tau] < threshold){
                    while(tau + 1 < maxlag && d[tau + 1] < d[tau]) tau++;
                    return tau;
                }
            }
            int globalMin = 2;
            for (int tau = 2; tau < maxlag; tau++){
                if (d[tau] < d[globalMin]) globalMin = tau;
            }
            return globalMin;
        }
        // Step - 6 (Parabolic Interpolation)
        db parabolicInterpol(int tau){
            if (tau < 1 || tau >= maxlag - 1) return (db)tau;
            db s0 = d[tau - 1];
            db s1 = d[tau];
            db s2 = d[tau + 1];

            db den = 2.0 * (s0 + s2 - 2.0 * s1);
            if (abs(den) < 1e-6) return (db)tau;     // Division by 0 prevention
            return (db)tau + (s0 - s2) / den;
        }

        void updateHistory(YINResult res){
            if (resultHistory.size() >= historySize) resultHistory.erase(resultHistory.begin());
            resultHistory.push_back(res);
        }
        // Step - 7 : Best Local Estimate (Done because real-world signals might contain noise)
        // For more accurate results, this function checks a short buffer of recent estimates and picks the one with the highest quality
        db getBestLocalTau(){
            if (resultHistory.empty()) return -1.0;
            auto best = min_element(resultHistory.begin(), resultHistory.end(), [](const YINResult& a, const YINResult& b){
                return a.quality < b.quality;
            });
            return best->tau;
        }
};

#endif