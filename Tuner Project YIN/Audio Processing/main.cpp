#include <iostream>
#include <vector>
#include <iomanip>
#include "portaudio.h"
#include "YIN_Estimator.hpp"

using namespace std;

struct Note {
    string name;
    double freq;
};

const vector<Note> guitarNotes = {
    {"E2 6th string", 82.41},
    {"A2 5th string", 110.00},
    {"D3 4th string", 146.83},
    {"G3 3rd string", 196.00},
    {"B3 2nd string", 246.94},
    {"E4 1st string", 329.63}
};

// This struct acts as the bridge between the hardware and the class
struct TunerData {
    YINEstimator* estimator;
    vdb slidingWindow;
};

string getTuningInstruction(double detectedFreq) {
    if (detectedFreq < 40.0) return "---";           // Keeps a lower bound on the freq detection

    // 1. Find the closest target note based on the determined frequency of the input signal (Standard Nearest Neighbor search)
    const Note* closest = &guitarNotes[0];
    double minDiff = abs(detectedFreq - guitarNotes[0].freq);

    for (const auto& note : guitarNotes) {
        double diff = abs(detectedFreq - note.freq);
        if (diff < minDiff) {
            minDiff = diff;
            closest = &note;
        }
    }

    // 2. Calculate instruction based on the difference
    double delta = detectedFreq - closest->freq;
    
    // We use a "Tolerance" of 0.5 Hz for being "In Tune"
    if (abs(delta) <= 1.0) {
        return "[ " + closest->name + " ] -> PERFECT!  ✅";
    } else if (delta < 0) {
        return "[ " + closest->name + " ] -> TUNE UP  ▲";
    } else {
        return "[ " + closest->name + " ] -> TUNE DOWN ▼";
    }
}

// Callback function
// This is the "Ear" function called by Windows/PortAudio automatically
static int audioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    TunerData *data = (TunerData*)userData;
    const float *in = (const float*)inputBuffer;

    if (inputBuffer == NULL) return paContinue;

    // 1. Shift the sliding window (The "Slide" logic)
    // We drop the oldest samples and add the new ones received from the mic
    data->slidingWindow.erase(data->slidingWindow.begin(), data->slidingWindow.begin() + framesPerBuffer);
    for(int i = 0; i < framesPerBuffer; i++) {
        data->slidingWindow.push_back((db)in[i]);
    }

    // 2. Runs the YIN algo on the current window
    db pitch = data->estimator->getPitch(data->slidingWindow);

    // 3. Simple UI: Update the terminal line without scrolling
    // if (pitch > 30.0) { 
    //     cout << "\r[ Live Tuner ] Detected: " << fixed << setprecision(2) << pitch << " Hz    " << flush;
    // } else {
    //     cout << "\r[ Live Tuner ] Listening...             " << flush;
    // }

    // 3. Professional UI Output
    if (pitch > 0) {
        string instruction = getTuningInstruction(pitch);
        
        // We add a "flush" for instant display and extra spaces to clear the line
        cout << "\r" << instruction << " (" << fixed << setprecision(1) << pitch << " Hz)    " << flush;
    } 
    else {
        cout << "\r[ TUNER ] Listening...                        " << flush;
    }
    return paContinue;
}

int main() {
    const int sampleRate = 44100;    // sampling rate => microphone captures 44100 discrete audio snapshots per second
    const int winSize = 1024;        // Number of samples YIN needs to make the pitch calculation
    

    // The third parameter sets the Absolute threshold(Step - 5) for the YIN Algorithm
    YINEstimator myTuner(sampleRate, winSize, 0.125);
    TunerData data;
    data.estimator = &myTuner;
    
    // Initialize the window with silence (3000 samples to cover window + max lag)
    data.slidingWindow.resize(3000, 0.0); 

    // Initialize PortAudio(Boots up the PortAudio engine)
    PaError err = Pa_Initialize();
    int defaultDevice = Pa_GetDefaultInputDevice();
    const PaDeviceInfo* info = Pa_GetDeviceInfo(defaultDevice);
    printf("\nUsing Device: %s\n", info->name);
    printf("Default Sample Rate: %f\n\n", info->defaultSampleRate);
    if (err != paNoError) return 1;

    // Configuring audio hardware and links it to the code
    PaStream *stream;
    // Open Microphone Stream (1 input channel, 44.1kHz, 512 samples per burst)
    // &stream -> A pointer that PortAudio will use allow the user to control the audio stream later
    // 1 : Number of i/p channels
    // 0 : Number of o/p channels
    // paFloat32 -> tells PortAudio to convert the microphone's raw electrical data into 32-bit floating-point numbers (values ranging from -1.0 to 1.0)
    // 512 : frames/samples per buffer, PortAudio will wait and collect 512 samples then fire the callback function
    // &data -> Memory address of the tuner data struct so the CB function knows where to put the audio
    err = Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, sampleRate, 512, audioCallback, &data);
    
    // Error check for PortAudio-hardware coordination
    if (err != paNoError) {
        cout << "PortAudio Error: " << Pa_GetErrorText(err) << endl;
        return 1;
    }
    // Asks PortAudio to turn on the microphone
    // In the BG : PortAudio creates a separate background thread. 
    // This thread continuously pulls audio from the mic and fires your audioCallback function roughly 86 times a second (44100 / 512)
    Pa_StartStream(stream);

    cout << "--- YIN Tuner CLI (Real-Time) ---" << endl;
    cout << "Listening to your microphone. Press Enter to exit." << endl;
    
    cin.get(); // Keep program alive while the background thread listens

    // Terminates the listening process on the press of "Enter"
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}