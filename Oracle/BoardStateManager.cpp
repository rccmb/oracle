#include "BoardStateManager.h"

DWORD WINAPI LetterRenderThread(LPVOID param) {
    while (true) {
        if (g_hasAnalysisStarted) {
            bool changed = false;

            // Print all of the queues. HERE
            // -----------------------------
            // Print current and previous queues as 8x8 grids
            // -----------------------------
            std::cout << "\n=== Current Queue (g_letterDrawQueue) ===\n";
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    size_t idx = row * 8 + col;
                    if (idx < g_letterDrawQueue.size())
                        std::cout << g_letterDrawQueue[idx] << ' ';
                    else
                        std::cout << ". ";
                }
                std::cout << "\n";
            }

            std::cout << "\n=== Previous Queue (g_prevLetterDrawQueue) ===\n";
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    size_t idx = row * 8 + col;
                    if (idx < g_prevLetterDrawQueue.size())
                        std::cout << g_prevLetterDrawQueue[idx] << ' ';
                    else
                        std::cout << ". ";
                }
                std::cout << "\n";
            }
            std::cout << "==========================================\n";


            // Populating the new queue.
            g_letterDrawQueue.clear();
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    char letter = g_detectedLetters[row * 8 + col];
                    g_letterDrawQueue.push_back( letter );
                }
            }

            // For each element in the new queue, compare with the old queue.
            if (g_letterDrawQueue.size() != g_prevLetterDrawQueue.size()) {
                changed = true;
            }
            else {
                for (size_t i = 0; i < g_letterDrawQueue.size(); ++i) {
                    if (g_letterDrawQueue[i] != g_prevLetterDrawQueue[i]) {
                        std::cout << g_letterDrawQueue[i] << " != " << g_prevLetterDrawQueue[i] << std::endl;
                        changed = true;
                        break;
                    }
                }
            }
            
            if (changed) {
                std::cout << "Change detected." << std::endl;
                g_prevLetterDrawQueue.clear();

                // Populating the previous queue with the current queue.
                for (int row = 0; row < 8; ++row) {
                    for (int col = 0; col < 8; ++col) {
                        char letter = g_letterDrawQueue[row * 8 + col];
                        g_prevLetterDrawQueue.push_back({ letter });
                    }
                }

                // Update board grid rows with the current detected letters.
                std::array<std::string, 8> newRows;
                for (int row = 0; row < 8; row++) {
                    std::string rowStr;
                    for (int col = 0; col < 8; col++) {
                        char letter = g_detectedLetters[row * 8 + col];
                        rowStr.push_back(letter);
                    }
                    g_boardGridRows[row] = rowStr;
                }

                std::lock_guard<std::mutex> boardLock(g_boardChangedMutex);
                g_boardChanged = true;
            }
            else {
                std::lock_guard<std::mutex> boardLock(g_boardChangedMutex);
                g_boardChanged = false;
            }
        }

        Sleep(10); // Throttle.
    }

    return 0;
}