# Basic Modules (Building Blocks) in Audio Signal Processing

You're referring to what's commonly called **"primitive modules"**, **"atomic DSP blocks"**, **"elemental stages"**, or **"functional blocks"** — the smallest indivisible processing units in a signal chain.

---

## 🔷 ANALOG DOMAIN

### **Signal Generation**
| Module | Function |
|--------|----------|
| Oscillator (VCO) | Generates periodic waveform (sine, saw, square, triangle) |
| Noise Generator | Generates white/pink/brown noise |
| Envelope Generator (ADSR) | Generates time-varying control signal |
| LFO | Low-frequency oscillation for modulation |
| Sample & Hold | Captures and holds instantaneous signal value |

---

### **Amplification & Gain**
| Module | Function |
|--------|----------|
| Amplifier (VCA) | Scales signal level, fixed or voltage-controlled |
| Attenuator | Reduces signal level passively |
| Buffer | Impedance isolation, unity gain |
| Inverter | Flips signal polarity (×-1) |
| Summing Amplifier | Adds multiple signals together |
| Difference Amplifier | Subtracts one signal from another |

---

### **Filtering**
| Module | Function |
|--------|----------|
| Low-Pass Filter (LPF) | Passes frequencies below cutoff |
| High-Pass Filter (HPF) | Passes frequencies above cutoff |
| Band-Pass Filter (BPF) | Passes a band of frequencies |
| Band-Reject / Notch Filter | Rejects a band of frequencies |
| All-Pass Filter (APF) | Passes all frequencies, shifts phase only |
| Shelving Filter | Boosts/cuts above or below a frequency |
| Peaking / Bell Filter | Boosts/cuts around a center frequency |

---

### **Time / Phase**
| Module | Function |
|--------|----------|
| Analog Delay Line (BBD) | Delays signal using bucket-brigade device |
| RC Delay / Phase Shift Network | Introduces phase shift at certain frequency |
| All-Pass Phase Shifter | Phase rotation without amplitude change |

---

### **Detection / Measurement**
| Module | Function |
|--------|----------|
| Envelope Detector (RMS / Peak) | Extracts amplitude envelope from signal |
| Level Detector | Detects signal crossing a threshold |
| Zero-Crossing Detector | Detects when signal crosses zero |
| Rectifier (Half/Full Wave) | Converts AC signal to unipolar |
| Peak Detector | Captures peak amplitude and holds it |
| Comparator | Outputs high/low depending on signal vs reference |

---

### **Modulation**
| Module | Function |
|--------|----------|
| Ring Modulator | Multiplies two signals (AM with no carrier) |
| AM Modulator | Amplitude modulation with carrier |
| VCA-based Modulator | Uses VCA for tremolo, AM effects |
| Multiplier Cell | Analog four-quadrant multiplier (e.g., Gilbert cell) |

---

### **Mixing / Routing**
| Module | Function |
|--------|----------|
| Passive Mixer | Resistive mixing, no gain |
| Active Mixer / Op-Amp Summer | Buffered summing with gain control |
| Crossfader | Blend between two signals |
| Switch / Mux | Routes signal to one of multiple paths |
| Panner | Distributes signal between left/right channels |

---

### **Impedance / Interface**
| Module | Function |
|--------|----------|
| Impedance Converter | Matches source to load impedance |
| Transformer | Galvanic isolation, impedance transformation |
| DI Buffer | Converts high-Z instrument to low-Z balanced |
| Pre-emphasis Network | Boosts high frequencies before recording |
| De-emphasis Network | Restores high frequencies after playback |

---

---

## 🔷 DIGITAL DOMAIN (DSP)

### **Signal Generation**
| Module | Function |
|--------|----------|
| Digital Oscillator (NCO/DDFS) | Numerically controlled oscillator via lookup table |
| Noise Generator (LFSR/PRNG) | Pseudo-random number sequence as noise |
| Envelope Generator | Digital ADSR state machine |
| LFO (digital) | Low-rate periodic modulation signal |
| Impulse Generator | Single-sample pulse, used for convolution/testing |
| DC Generator | Constant value output (offset source) |

---

### **Gain & Amplitude**
| Module | Function |
|--------|----------|
| Multiplier | Scales sample by coefficient |
| Adder / Summer | Adds two or more sample streams |
| Subtractor | Subtracts one signal from another |
| Accumulator | Running sum of samples |
| Gain Smoother | Prevents clicks by interpolating gain changes |
| Clipper (Hard/Soft) | Limits signal at a threshold |
| Normalizer | Scales signal to a target peak/RMS level |

---

### **Delay & Memory**
| Module | Function |
|--------|----------|
| Unit Delay (z⁻¹) | Single-sample delay, fundamental DSP block |
| Delay Line (Circular Buffer) | Variable-length sample buffer |
| Fractional Delay (Interpolated) | Sub-sample delay using interpolation |
| Feedback Tap | Routes delayed output back into input |
| Feedforward Tap | Takes a copy of delayed signal forward |

---

### **Filtering**
| Module | Function |
|--------|----------|
| FIR Filter Kernel | Finite impulse response, sum of weighted taps |
| IIR Biquad Section | 2nd-order recursive filter (most common DSP filter unit) |
| Comb Filter | Delay with feedback or feedforward creating periodic notches |
| Allpass Section | Phase-shifting filter, unity magnitude response |
| DC Blocker | High-pass to remove DC offset |
| Integrator | Accumulates signal (low-pass behavior) |
| Differentiator | Measures sample difference (high-pass behavior) |

---

### **Modulation**
| Module | Function |
|--------|----------|
| Multiplier (Ring Mod) | Multiplies signal by modulator sample-by-sample |
| Phase Modulator | Modulates read pointer of oscillator or delay |
| Frequency Modulator | Modulates oscillator frequency |
| Tremolo Module | LFO multiplied into signal amplitude |
| Sample Scrubber | Reads buffer at variable/modulated rate |

---

### **Detection / Analysis**
| Module | Function |
|--------|----------|
| Peak Follower | Tracks peak amplitude with attack/release |
| RMS Detector | Calculates root mean square level over window |
| Envelope Follower | Smooth tracking of amplitude over time |
| Level Comparator | Compares instantaneous level to threshold |
| Zero-Crossing Detector | Detects sign changes between samples |
| Differentiator (slope detector) | Detects rate of change in signal |
| Autocorrelation Block | Measures signal periodicity (used in pitch detection) |

---

### **Interpolation (for fractional operations)**
| Module | Function |
|--------|----------|
| Linear Interpolator | Weighted average between two samples |
| Cubic / Hermite Interpolator | Smooth curve fitting between samples |
| Sinc Interpolator | Band-limited ideal interpolation |
| Lagrange Interpolator | Polynomial-based interpolation |

---

### **Sample Rate / Format Conversion**
| Module | Function |
|--------|----------|
| Upsampler (Interpolator) | Inserts zeros then filters (increases sample rate) |
| Downsampler (Decimator) | Filters then drops samples (reduces sample rate) |
| Anti-Aliasing Filter | LPF before downsampling |
| Anti-Imaging Filter | LPF after upsampling |
| Format Converter | e.g., float ↔ integer, 24-bit ↔ 32-bit |

---

### **Frequency Domain (FFT-based)**
| Module | Function |
|--------|----------|
| FFT Block | Converts time-domain block to frequency domain |
| IFFT Block | Converts frequency domain back to time domain |
| Window Function | Applies Hann, Hamming, etc. before FFT |
| Spectral Multiplier | Multiplies complex FFT bins (convolution in time domain) |
| Phase Vocoder Frame | Analysis/synthesis frame for pitch/time stretching |
| Overlap-Add / Overlap-Save Block | Manages FFT frames for continuous processing |

---

### **Mixing / Routing**
| Module | Function |
|--------|----------|
| Digital Crossfader | Interpolates between two signal streams |
| Wet/Dry Mixer | Blends processed and unprocessed signal |
| Matrix Mixer | Routes N inputs to M outputs with coefficients |
| Stereo Panner | Distributes mono signal to stereo field |
| Mid-Side Encoder/Decoder | Converts L/R to M/S and back |

---

### **Nonlinear / Distortion**
| Module | Function |
|--------|----------|
| Waveshaper | Applies transfer function/lookup table to sample value |
| Hard Clipper | Truncates samples above threshold |
| Soft Clipper | Smooth saturation curve (tanh, arctan, etc.) |
| Bit Crusher | Reduces bit depth by requantization |
| Sample Rate Reducer | Holds samples to simulate lower sample rate |

---

## 🔁 Summary: Analog vs Digital Equivalence

| Analog Module | Digital Equivalent |
|---------------|-------------------|
| RC Integrator | IIR One-Pole Low-Pass |
| RC Differentiator | FIR/IIR High-Pass |
| BBD Delay | Circular Buffer + Delay Line |
| Envelope Detector | Peak/RMS Follower |
| VCA | Digital Multiplier |
| Analog Summer | Digital Adder |
| All-Pass Phase Shift | Digital All-Pass Biquad |
| Pre/De-emphasis RC | Shelving Filter |

---

This covers essentially **all the atomic building blocks** that more complex processors (compressors, reverbs, choruses, vocoders, etc.) are assembled from. Would you like me to go deeper into any specific category?
 
