#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <cmath>
const double PI = acos(-1.0); 
using namespace std;

int main() {
    const int sampleRate = 44100;
    const double targetFreq = 441.0; // Perfect 100-sample period
    const double duration = 0.2;     // 200ms of audio
    const int numSamples = sampleRate * duration;

    ofstream outFile("signal.txt");
    if (!outFile) {
        cerr << "Error opening file!" << endl;
        return 1;
    }

    for (int i = 0; i < numSamples; ++i) {
        // Generate pure sine wave: sin(2 * pi * f * t)
        double t = (double)i / sampleRate;
        double sample = sin(2.0 * PI * targetFreq * t);

        // Add 1% white noise to test Step 6 stability
        double noise = ((double)rand() / RAND_MAX - 0.5) * 0.02;
        
        outFile << (sample + noise) << "\n";
    }

    outFile.close();
    cout << "Generated " << numSamples << " samples in signal.txt" << endl;
    return 0;
}