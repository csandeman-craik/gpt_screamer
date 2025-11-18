#pragma once
#include <cmath>

struct DiodePair {
    // Tube Screamer uses two 1N4148s antiparallel
    double Is = 2.52e-9;
    double nVt = 1.906 * 0.02585; // n * thermal voltage
    double Rpar = 1e-8;           // tiny parallel to stabilize Newton

    double nonlinearSolve(double Vin) {
        double V = 0.0;
        for (int i = 0; i < 8; ++i) {
            double I1 = Is * (std::exp( V / nVt ) - 1.0);
            double I2 = Is * (std::exp(-V / nVt ) - 1.0);
            double f  = V / Rpar + (I1 - I2) - Vin;
            double df = 1.0 / Rpar + (I1 + I2) / nVt;
            V -= f / df;
        }
        return V;
    }
};

class GptScreamer {
public:
    GptScreamer() = default;
    GptScreamer(double fs) : fs(fs) {
        setFs(fs);
    }

    void setFs(double fs) {
        this->fs = fs;
        // bilinear-transform of caps
        aC1 = 2.0 * fs * 47e-9;      // input 47n
        aC2 = 2.0 * fs * 220e-9;     // output 220n
    }

    float process(float x) {
        // ---- HIGH-PASS INPUT FILTER ----
        double vHP = hpFilter(x);

        // ---- LINEAR OP-AMP GAIN ----
        double vOp = gainStage(vHP);

        // ---- NONLINEAR FEEDBACK (DIODE PAIR) ----
        double vClip = diode.nonlinearSolve(vOp);

        // ---- LOW-PASS OUTPUT FILTER ----
        double y = lpFilter(vClip);

        return (float)y;
    }

private:
    double fs;
    double aC1, aC2;

    DiodePair diode;

    // Simple matched filters split out from the WDF tree
    double zHP = 0;
    double hpFilter(double x) {
        // HP: R1=470k, C1=47n
        double R = 470000.0;
        double y = (aC1 * (x - zHP) + x * R) / (R + aC1);
        zHP = y;
        return y;
    }

    double gainStage(double v) {
        // TS op-amp gain = 1 + R3/R2 = 1 + 51k / 4.7k ≈ 11.85
        return v * 11.85;
    }

    double zLP = 0;
    double lpFilter(double x) {
        // LP: R4=1k, C3=220n
        double R = 1000.0;
        double y = (x * R + aC2 * zLP) / (R + aC2);
        zLP = y;
        return y;
    }
};
