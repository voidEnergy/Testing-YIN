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
    vector <YINResult> resultHistory;
    const int historySize = 5; 
    
    public:
        YINEstimator(int sr, int winSize, db thold = 0.1) : threshold(thold), sampleRate(sr), winsize(winSize) {
            maxlag = sr / 40;
            d.resize(maxlag, 0.0);
        }

        db getPitch(const vdb &buffer) {
            if (buffer.size() < (size_t)(winsize + maxlag)) return -1.0;

            // 1. SILENCE GATE (RMS)
            db sum = 0;
            for (db sample : buffer) sum += sample * sample;
            db rms = sqrt(sum / buffer.size());

            // 2. YIN MATH
            difference(buffer);
            CMNDF(); 

            // 3. CANDIDATE SEARCH
            int tauEstimate = absThreshold();
            db quality = (tauEstimate != -1) ? d[tauEstimate] : 1.0;

            // 4. THROTTLED DEBUG DISPLAY
            static int debugCounter = 0;
            if (++debugCounter % 20 == 0) {
                printf("\r[DEBUG] RMS: %.4f | Quality: %.4f | Tau: %d    ", rms, quality, tauEstimate);
                fflush(stdout);
            }

            // 5. THE GATEKEEPERS
            if (rms < 0.005) return -1.0;      
            if (tauEstimate == -1) return -1.0; 
            if (quality > 0.5) return -1.0;    

            // 6. REFINEMENT
            db refinedTau = parabolicInterpol(tauEstimate);
            updateHistory({refinedTau, quality});
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
        void difference(const vdb &buffer){
            fill(d.begin(), d.end(), 0.0);
            for (int tau = 1; tau < maxlag; tau++){
                for(int j = 0; j < winsize; j++){
                    db diff = buffer[j] - buffer[j + tau];
                    d[tau] += diff * diff;
                }
            }
        }

        void CMNDF(){
            d[0] = 1.0;
            db runningSum = 0.0;
            for (int tau = 1; tau < maxlag; tau++){
                runningSum += d[tau];
                d[tau] /= (runningSum / tau);
            }
        }

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

        db parabolicInterpol(int tau){
            if (tau < 1 || tau >= maxlag - 1) return (db)tau;
            db s0 = d[tau - 1];
            db s1 = d[tau];
            db s2 = d[tau + 1];

            db den = 2.0 * (s0 + s2 - 2.0 * s1);
            if (abs(den) < 1e-6) return (db)tau;
            return (db)tau + (s0 - s2) / den;
        }

        void updateHistory(YINResult res){
            if (resultHistory.size() >= historySize) resultHistory.erase(resultHistory.begin());
            resultHistory.push_back(res);
        }

        db getBestLocalTau(){
            if (resultHistory.empty()) return -1.0;
            auto best = min_element(resultHistory.begin(), resultHistory.end(), [](const YINResult& a, const YINResult& b){
                return a.quality < b.quality;
            }); // FIXED: Added missing closing bracket and semicolon
            return best->tau;
        }
};

#endif