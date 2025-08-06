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
    T firstValue, T secondValue,
    size_t byteRange)
{
    const size_t N = sizeof(T);
    // how many “starts” we can read
    const size_t totalStarts = (buffer.size() >= N ? buffer.size() - N + 1 : 0);

    // 1) First pass: find all positions where buffer[i]==firstValue
    std::vector<size_t> candidates;
    candidates.reserve(1024);
    for (size_t i = 0; i < totalStarts; ++i) {
        T curr;
        std::memcpy(&curr, &buffer[i], N);
        if (curr == firstValue)
            candidates.push_back(i);
    }

    // 2) Count how many inner‐checks we’ll do
    size_t phase1Steps = totalStarts;
    size_t phase2Steps = 0;
    for (size_t i : candidates) {
        size_t start = (i > byteRange ? i - byteRange : 0);
        size_t end = std::min(buffer.size() - N, i + byteRange);
        phase2Steps += (end - start + 1);
    }
    size_t totalSteps = phase1Steps + phase2Steps;

    // 3) Set up a bar‐drawing lambda
    const int BAR_WIDTH = 50;
    size_t workDone = 0;
    size_t lastPct = 0;

    auto drawBar = [&]() {
        size_t pct = workDone * 100 / totalSteps;
        if (pct != lastPct) {
            size_t pos = pct * BAR_WIDTH / 100;
            std::cout << '\r' << '[';
            for (int x = 0; x < BAR_WIDTH; ++x)
                std::cout << (x < pos ? '=' : ' ');
            std::cout << "] " << std::setw(3) << pct << '%' << std::flush;
            lastPct = pct;
        }
        };

    // 4) Initial empty bar
    std::cout << '\n' << '[' << std::string(BAR_WIDTH, ' ') << "]   0%" << std::flush;

    // 5) Phase 1: tick for every start
    for (size_t i = 0; i < totalStarts; ++i) {
        ++workDone;
        drawBar();
    }

    // 6) Phase 2: do each inner‐loop check and record matches
    std::vector<std::pair<size_t, size_t>> found;
    for (size_t i : candidates) {
        size_t start = (i > byteRange ? i - byteRange : 0);
        size_t end = std::min(buffer.size() - N, i + byteRange);
        for (size_t j = start; j <= end; ++j) {
            ++workDone;
            drawBar();

            T tmp;
            std::memcpy(&tmp, &buffer[j], N);
            if (tmp == secondValue)
                found.emplace_back(i, j);
        }
    }

    // 7) Ensure we’ve hit 100%
    workDone = totalSteps;
    drawBar();

    // 8) Erase the bar line (cover `[=====]100%`)
    std::cout << '\r' << std::string(BAR_WIDTH + 2 + 1 + 3 + 1 + 2, ' ') << '\r';

    // 9) Print results
    if (found.empty())
    {
        std::cout << "No matches found within " << byteRange << " bytes.\n";
    }
    else
    {
        // Print count (singular vs. plural)
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
    const auto BAR_WIDTH     = 40;
    const auto SHOW_AFTER_MS = 100;      // ms before the bar appears
    const auto CHUNK_SIZE    = 1 << 20;  // 1 MiB per read

    while (true)
    {
        std::cout << prompt;
        std::string fileName;
        if (!std::getline(std::cin, fileName))
            std::exit(1);

        std::ifstream file(fileName, std::ios::binary | std::ios::ate);
        if (!file)
        {
            std::cerr << "Could not open \"" << fileName << "\". Try again.\n\n";
            continue;
        }

        // 1) Get total size
        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buf;
        buf.reserve(size_t(fileSize));

        // 2) Prepare for timed reads
        auto startTime = std::chrono::steady_clock::now();
        bool showBar = false;
        size_t bytesRead = 0, lastPct = 0;

        // 3) Read in chunks
        while (bytesRead < size_t(fileSize))
        {
            size_t toRead = std::min<size_t>(CHUNK_SIZE, size_t(fileSize) - bytesRead);
            buf.resize(bytesRead + toRead);

            file.read(buf.data() + bytesRead, toRead);
            size_t justRead = file.gcount();
            if (!justRead) break;

            bytesRead += justRead;

            // 4) Decide if we should show the progress bar yet
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();

            if (elapsed > SHOW_AFTER_MS || showBar)
            {
                if (!showBar)
                {
                    // first time we decide to show progress:
                    // print "Loading file..." plus newline, then draw the empty bar
                    std::cout << "\rLoading file... ["
                        << std::string(BAR_WIDTH, ' ')
                        << "]   0%" << std::flush;
                    showBar = true;
                }

                // update bar exactly as before, with '\r' then [===] XX%
                size_t pct = bytesRead * 100 / size_t(fileSize);
                if (pct != lastPct)
                {
                    size_t pos = pct * BAR_WIDTH / 100;
                    std::cout << '\r' << "Loading file... [";
                    for (int x = 0; x < BAR_WIDTH; ++x)
                        std::cout << (x < pos ? '=' : ' ');
                    std::cout << "] " << std::setw(3) << pct << '%' << std::flush;
                    lastPct = pct;
                }
            }
        }

        std::cout << '\r'
            << std::string(BAR_WIDTH + /*brackets*/2
                + /*space*/1
                + /*digits*/3
                + /*percent*/1
                + /*"Loading file... [] "%*/16, ' ')
            << '\r'
            << std::flush;

        // THEN emit a newline so the menu prints on line 2
        std::cout << '\n';

        // 6) If we read the whole file, return it
        if (bytesRead == size_t(fileSize))
            return buf;

        // Otherwise something went wrong
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
