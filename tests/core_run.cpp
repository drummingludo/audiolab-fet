// Fait tourner le moteur C++ du plugin sur le même signal que ref_run.js.
// Usage : core_run <fs> <params.txt> <in.f32> <out.f32> [bloc] [trace]
// « trace » : écrit sur la sortie standard la réduction de gain (dB) de chaque bloc
// params.txt : inDb outDb atk rel ratio
#include "../Source/FetCore.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main (int argc, char** argv)
{
    if (argc < 5) return 1;
    const double fs = atof (argv[1]);
    const int block = argc > 5 ? atoi (argv[5]) : 128;
    const bool trace = argc > 6;
    audiolab::FetParams p;
    FILE* pf = fopen (argv[2], "r");
    if (fscanf (pf, "%lf %lf %lf %lf %d", &p.inDb, &p.outDb, &p.atk, &p.rel, &p.ratio) != 5) return 2;
    fclose (pf);
    FILE* f = fopen (argv[3], "rb");
    fseek (f, 0, SEEK_END); long bytes = ftell (f); fseek (f, 0, SEEK_SET);
    std::vector<float> in ((size_t) bytes / 4);
    if (fread (in.data(), 4, in.size(), f) != in.size()) return 3;
    fclose (f);
    const int N = (int) in.size() / 2;
    std::vector<float> out (in.size()), L (block), R (block), oL (block), oR (block);
    audiolab::FetCore c;
    c.prepare (fs);
    c.setParams (p);
    for (int s = 0; s < N; s += block)
    {
        const int n = std::min (block, N - s);
        for (int i = 0; i < n; ++i) { L[i] = in[2 * (s + i)]; R[i] = in[2 * (s + i) + 1]; }
        c.process (L.data(), R.data(), oL.data(), oR.data(), n);
        if (trace) printf ("%.9g\n", (double) c.takeGRMin());
        for (int i = 0; i < n; ++i) { out[2 * (s + i)] = oL[i]; out[2 * (s + i) + 1] = oR[i]; }
    }
    FILE* o = fopen (argv[4], "wb");
    fwrite (out.data(), 4, out.size(), o);
    fclose (o);
    return 0;
}
