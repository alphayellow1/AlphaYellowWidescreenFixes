#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <limits>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <algorithm>

// Helper: read one value of type T from stdin, retrying on failure
template<typename T>
T readValue(const std::string& prompt)
{
    T v;

    while (true)
    {
        std::cout << prompt;

        if (std::cin >> v)
        {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            return v;
        }

        std::cin.clear();

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        std::cout << "Invalid, please enter a value of the correct type.\n";
    }
}

// Core search loop, parameterized on T
template<typename T>
void findPairs(const std::vector<char>& buffer,
    T firstValue,
    T secondValue,
    size_t byteRange)
{
    const size_t N = sizeof(T);
    const size_t totalStarts = (buffer.size() >= N ? buffer.size() - N + 1 : 0);

    // ——— DRY RUNS ———————————————————————————————————————————————
    std::vector<size_t> initialCandidates;
    initialCandidates.reserve(1024);
    for (size_t i = 0; i < totalStarts; ++i) {
        T curr;
        std::memcpy(&curr, &buffer[i], N);
        if (curr == firstValue)
            initialCandidates.push_back(i);
    }

    size_t phase1Steps = totalStarts;
    size_t phase2Steps = 0;
    for (size_t i : initialCandidates) {
        size_t start = (i > byteRange ? i - byteRange : 0);
        size_t end = std::min(buffer.size() - N, i + byteRange);
        phase2Steps += (end - start + 1);
    }
    const size_t totalSteps = phase1Steps + phase2Steps;

    // ——— PROGRESS BAR SETUP ——————————————————————————————————————
    const int  BAR_WIDTH = 50;
    size_t     workDone = 0;
    size_t     lastPos = SIZE_MAX;   // forces initial draw
    const std::string PREFIX = "Scanning for matches... ";

    auto drawBar = [&](bool force = false) {
        if (totalSteps == 0) return;
        double frac = double(workDone) / double(totalSteps);
        size_t pos = size_t(frac * BAR_WIDTH + 0.5);
        if (force || pos != lastPos) {
            std::cout << '\r' << PREFIX << '[';
            for (int x = 0; x < BAR_WIDTH; ++x)
                std::cout << (x < pos ? '=' : ' ');
            std::cout << "] "
                << std::setw(3) << int(frac * 100 + 0.5)
                << '%' << std::flush;
            lastPos = pos;
        }
        };

    // one-line prefix+bar
    std::cout << '\n';
    drawBar(/*force=*/true);

    // ——— SCAN PASSES ——————————————————————————————————————————————
    std::vector<size_t> candidates;
    candidates.reserve(initialCandidates.size());
    for (size_t i = 0; i < totalStarts; ++i) {
        T curr;
        std::memcpy(&curr, &buffer[i], N);
        if (curr == firstValue)
            candidates.push_back(i);

        ++workDone;
        drawBar();
    }

    std::vector<std::pair<size_t, size_t>> found;
    for (size_t i : candidates) {
        size_t start = (i > byteRange ? i - byteRange : 0);
        size_t end = std::min(buffer.size() - N, i + byteRange);
        for (size_t j = start; j <= end; ++j) {
            T tmp;
            std::memcpy(&tmp, &buffer[j], N);
            if (tmp == secondValue)
                found.emplace_back(i, j);

            ++workDone;
            drawBar();
        }
    }

    // ——— FINISH AT 100% ————————————————————————————————————————
    workDone = totalSteps;
    drawBar(/*force=*/true);

    // clear the entire line (prefix + bar + percent)
    size_t clearLen = PREFIX.size() + BAR_WIDTH + 7;
    std::cout << '\r' << std::string(clearLen, ' ') << '\r';

    // ——— REPORT RESULTS ——————————————————————————————————————
    if (found.empty())
    {
        std::cout << "No matches found within " << byteRange << " bytes.\n";
    }
    else
    {
        std::cout << "Found " << found.size() << (found.size() == 1 ? " pair" : " pairs") << " within " << byteRange << " bytes at the following addresses:\n";
        for (auto& p : found)
        {
            std::cout << "First value (" << firstValue
                << ") at: " << std::right << std::setw(8) << std::setfill('0') << std::hex << std::uppercase
                << p.first << std::dec << std::setfill(' ')
                << ", Second value (" << secondValue
                << ") at: " << std::right << std::setw(8) << std::setfill('0') << std::hex << std::uppercase
                << p.second << std::dec << std::setfill(' ')
                << "\n";
        }
    }
}

template<typename T>
void handleTypeSearch(const std::vector<char>& buffer)
{
    // build the prompts dynamically
    auto firstPrompt = "\n- Enter the first value: ";

    auto secondPrompt = "\n- Enter the second value: ";

    auto rangePrompt = "\n- Enter the byte range: ";

    T first = readValue<T>(firstPrompt);

    T second = readValue<T>(secondPrompt);

    size_t range = readValue<size_t>(rangePrompt);

    findPairs<T>(buffer, first, second, range);
}

template<typename T> T readInput(T minValue, T maxValue)
{
    std::string line;

    while (true)
    {
        if (!std::getline(std::cin, line))
        {
            std::exit(1);  // EOF or error
        }

        std::stringstream ss(line);

        T value;

        char extra;

        // parse T, reject if there are leftover chars, enforce bounds
        if (ss >> value && !(ss >> extra) && value >= minValue && value <= maxValue)
        {
            return value;
        }

        std::cout << "Please enter a value between " << minValue << " and " << maxValue << ".\n";
    }
}

std::vector<char> readFileBuffer(const std::string& prompt)
{
    const int    BAR_WIDTH = 40;
    const size_t CHUNK_SIZE = 1 << 20;  // 1 MiB
    const std::string LOADING_TEXT = "Loading file... ";

    while (true)
    {
        // 1) ask for filename
        std::cout << prompt;
        std::string fileName;
        if (!std::getline(std::cin, fileName))
            std::exit(1);

        // 2) open it
        std::ifstream file(fileName, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Could not open \"" << fileName << "\". Try again.\n\n";
            continue;
        }

        // 3) determine size
        size_t fileSize = size_t(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<char> buf;
        buf.reserve(fileSize);

        // 4) progress state
        size_t bytesRead = 0;
        size_t lastPos = SIZE_MAX;
        const size_t totalSteps = fileSize;

        auto drawBar = [&](bool force = false) {
            if (totalSteps == 0) return;               // nothing to do
            double frac = double(bytesRead) / totalSteps;
            size_t pos = size_t(frac * BAR_WIDTH + 0.5);
            if (force || pos != lastPos) {
                // print prefix + bar + percent
                std::cout << '\r' << LOADING_TEXT << '[';
                for (int i = 0; i < BAR_WIDTH; ++i)
                    std::cout << (i < pos ? '=' : ' ');
                std::cout << "] "
                    << std::setw(3) << int(frac * 100 + 0.5)
                    << '%' << std::flush;
                lastPos = pos;
            }
            };

        // 5) initial draw at 0%
        std::cout << '\n';
        drawBar(/*force=*/true);

        // 6) read in chunks, updating bar
        while (bytesRead < fileSize) {
            size_t toRead = std::min(CHUNK_SIZE, fileSize - bytesRead);
            buf.resize(bytesRead + toRead);
            file.read(buf.data() + bytesRead, toRead);
            size_t justRead = size_t(file.gcount());
            if (justRead == 0) break;
            bytesRead += justRead;
            drawBar();
        }

        // 7) force 100% and then fully clear that entire line
        bytesRead = fileSize;
        drawBar(/*force=*/true);

        // total length = prefix + '[' + BAR_WIDTH + ']' + space + "100%" = 
        // LOADING_TEXT.size() + 1 + BAR_WIDTH + 1 + 1 + 4
        size_t clearLen = LOADING_TEXT.size() + BAR_WIDTH + 7;
        std::cout << '\r'
            << std::string(clearLen, ' ')
            << '\r';

        // 8) done?
        if (bytesRead == fileSize)
        {
            return buf;
        }           

        std::cerr << "Error reading \"" << fileName << "\". Try again.\n";
    }
}

int main()
{
    std::cout << "==== Resolution Bytes Finder v1.1 by AlphaYellow, 2025 ====\n\n";

    auto buffer = readFileBuffer("- Enter file name: ");

    char again = '2';

    do
    {
        // 1) choose data type
        enum TypeID {UINT8 = 1, INT16, UINT16, INT32, UINT32, FLOAT, DOUBLE, INT64, UINT64};

        std::cout << "- Choose a data type:\n"
            << "  Integer types:\n"
            << "    1)  8-bit unsigned integer (uint8_t)    2) 16-bit signed integer (int16_t)\n"
            << "    3) 16-bit unsigned integer (uint16_t)   4) 32-bit signed integer (int32_t)\n"
            << "    5) 32-bit unsigned integer (uint32_t)   6) 64-bit signed integer (int64_t)\n"
            << "    7) 64-bit unsigned integer (uint64_t)\n"
            << "  Floating-point types:\n"
            << "    8) 32-bit floating point (float)        9) 64-bit floating point (double)\n"
            << "Selection> ";
        
        int selectedType = readInput<int>(1, 9);

        // 2) dispatch templated helper
        switch (selectedType)
        {
        case UINT8:
            handleTypeSearch<uint8_t>(buffer);
            break;

        case INT16:
            handleTypeSearch<int16_t>(buffer);
            break;

        case UINT16:
            handleTypeSearch<uint16_t>(buffer);
            break;

        case INT32:
            handleTypeSearch<int32_t>(buffer);
            break;

        case UINT32:
            handleTypeSearch<uint32_t>(buffer);
            break;

        case INT64:
            handleTypeSearch<int64_t>(buffer);
            break;

        case UINT64:
            handleTypeSearch<uint64_t>(buffer);
            break;

        case FLOAT:
            handleTypeSearch<float>(buffer);
            break;

        case DOUBLE:
            handleTypeSearch<double>(buffer);
            break;

        default:
            std::cout << "Unknown selection.\n";
            break;
        }

        // 3) ask to try again or exit
        std::cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        
        again = readInput<char>('1', '2');

        std::cout << "\n";
    } while (again == '2');

    std::cout << "Press Enter to exit...";

    std::cin.get();

    return 0;
}
