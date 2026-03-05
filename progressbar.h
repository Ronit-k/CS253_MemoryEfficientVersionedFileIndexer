/*
 * ProgressBar – A simple terminal progress bar for file processing.
 * Writes to stderr so it does not interfere with program output on stdout.
 *
 * Usage:
 *   ProgressBar bar(totalBytes, "Indexing v1");
 *   while (readChunk) { bar.update(chunkSize); }
 *   bar.finish();
 */

#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <iostream>
#include <string>

class ProgressBar {
private:
    size_t totalBytes;
    size_t processedBytes;
    int    barWidth;
    std::string label;
    int    lastPercent;

public:
    // totalBytes  – expected total size (bytes); 0 means indeterminate
    // lbl         – label printed before the bar
    // width       – number of characters for the filled/unfilled bar
    ProgressBar(size_t total,
                const std::string& lbl = "Processing",
                int width = 40)
        : totalBytes(total),
          processedBytes(0),
          barWidth(width),
          label(lbl),
          lastPercent(-1) {}

    // Call after each chunk is read. bytesJustRead = size of that chunk.
    void update(size_t bytesJustRead) {
        processedBytes += bytesJustRead;

        int percent = (totalBytes > 0)
            ? static_cast<int>((processedBytes * 100) / totalBytes)
            : 0;
        if (percent > 100) percent = 100;

        // Redraw only when the displayed percentage actually changes
        if (percent == lastPercent) return;
        lastPercent = percent;

        int filled = (barWidth * percent) / 100;

        std::cerr << "\r" << label << " [";
        for (int i = 0; i < barWidth; ++i) {
            if (i < filled)
                std::cerr << '#';
            else
                std::cerr << '-';
        }
        std::cerr << "] " << percent << "%" << std::flush;
    }

    // Ensure the bar shows 100 % and move to a new line.
    void finish() {
        if (totalBytes > 0) {
            processedBytes = totalBytes;
            lastPercent = -1;   // force redraw
            update(0);
        }
        std::cerr << "\n";
    }
};

#endif // PROGRESSBAR_H
