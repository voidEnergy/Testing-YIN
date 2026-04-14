#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "YINEstimator.hpp" // Include your header

using namespace std;

int main() {
    const int sampleRate = 44100;
    const int winSize = 1024;
    const double targetFreq = 440.0; // Concert A
    
    YINEstimator tuner(sampleRate, winSize);

    // Create a buffer for a 440Hz note (approx 100ms of audio)
    // Buffer must be > winSize + maxLag (approx 2200 samples)
    vector<double> buffer(4096);
    for (int i = 0; i < (int)buffer.size(); ++i) {
        // Generate sine wave: sin(2 * pi * f * t)
        buffer[i] = sin(2.0 * M_PI * targetFreq * i / sampleRate);
        
        // Add tiny bit of noise to simulate a real cable
        buffer[i] += ((double)rand() / RAND_MAX - 0.5) * 0.01;
    }

    double result = tuner.getPitch(buffer);

    cout << fixed << setprecision(2);
    cout << "--- YIN Algorithm CLI Test ---" << endl;
    if (result > 0) {
        cout << "Detected Frequency: " << result << " Hz" << endl;
        cout << "Target:             " << targetFreq << " Hz" << endl;
        cout << "Accuracy Error:     " << abs(result - targetFreq) << " Hz" << endl;
    } else {
        cout << "Status: No pitch detected. Check buffer size/threshold." << endl;
    }

    return 0;
}