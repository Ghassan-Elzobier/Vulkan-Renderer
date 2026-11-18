#include "image/compare.h"
#include "image/image_io.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

// Compares a rendered image with a reference and reports how far apart they are.
//
// Usage: Image_Compare reference.png test.png [--max-rmse X] [--diff diff.png] [--gain N]
//
// Exits 0 when the RMSE is at most --max-rmse (default 0, meaning "identical"), 1 when it is
// higher, and 2 on an error, so a script can use it as a pass or fail check.

int main(int argc, char** argv) {
    try {
        std::string paths[2];
        int pathCount = 0;
        double maxRmse = 0.0;
        std::string diffPath;
        int gain = 16;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--max-rmse" && i + 1 < argc) {
                maxRmse = std::atof(argv[++i]);
            } else if (arg == "--diff" && i + 1 < argc) {
                diffPath = argv[++i];
            } else if (arg == "--gain" && i + 1 < argc) {
                gain = std::atoi(argv[++i]);
            } else if (pathCount < 2 && arg.rfind("--", 0) != 0) {
                paths[pathCount++] = arg;
            } else {
                std::cerr << "Unknown argument '" << arg << "'\n";
                return 2;
            }
        }
        if (pathCount != 2) {
            std::cerr << "Usage: Image_Compare reference.png test.png [--max-rmse X] [--diff diff.png] [--gain N]\n";
            return 2;
        }

        const Image a = loadImage(paths[0]);
        const Image b = loadImage(paths[1]);
        const ImageDiff diff = compareImages(a, b);

        std::printf("RMSE: %.4f (0-255 scale)\n", diff.rmse);
        std::printf("Max channel difference: %d\n", diff.maxDiff);
        std::printf("Pixels differing: %llu of %llu (%.3f%%)\n", (unsigned long long)diff.pixelsDiffering,
                    (unsigned long long)diff.pixels, 100.0 * double(diff.pixelsDiffering) / double(diff.pixels));

        if (!diffPath.empty()) {
            saveImage(diffPath, diffImage(a, b, gain));
            std::printf("Diff image: %s (x%d)\n", diffPath.c_str(), gain);
        }

        const bool pass = diff.rmse <= maxRmse;
        std::printf("%s: RMSE %.4f %s %.4f\n", pass ? "PASS" : "FAIL", diff.rmse, pass ? "<=" : ">", maxRmse);
        return pass ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
