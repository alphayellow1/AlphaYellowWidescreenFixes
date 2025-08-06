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

using namespace std;

// Helper: read one value of type T from stdin, retrying on failure
template<typename T>
T readValue(const string& prompt)
{
    T v;

    while (true)
    {
        cout << prompt;

        if (cin >> v)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            return v;
        }

        cin.clear();

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        cout << "Invalid, please enter a value of the correct type.\n";
    }
}

// Core search loop, parameterized on T
template<typename T>
void findPairs(const vector<char> &buffer, T firstValue, T secondValue, size_t byteRange)
{
    const size_t N = sizeof(T);

    vector<pair<size_t,size_t>> found;

    for (size_t i = 0; i + N <= buffer.size(); ++i)
    {
        T curr;

        memcpy(&curr, &buffer[i], N);

        if (curr == firstValue)
        {
            size_t start = (i > byteRange ? i - byteRange : 0);

            size_t end = min(buffer.size() - N, i + byteRange);

            for (size_t j = start; j <= end; ++j)
            {
                T tmp;

                memcpy(&tmp, &buffer[j], N);

                if (tmp == secondValue)
                {
                    found.emplace_back(i, j);
                }
            }
        }
    }

    size_t count = found.size();

    if (count == 0)
    {
        cout << "No matches found within " << byteRange << " bytes.\n";
        return;
    }

    // Print count (singular vs. plural)
    cout << "\nFound " << count << (count == 1 ? " pair" : " pairs") << " within " << byteRange << " bytes at the following addresses:\n";

    for (auto& p : found)
    {
        cout << "First value (" << firstValue
            << ") at: " << right << setw(8) << setfill('0') << hex << uppercase
            << p.first << dec << setfill(' ')
            << ", Second value (" << secondValue
            << ") at: " << right << setw(8) << setfill('0') << hex << uppercase
            << p.second << dec << setfill(' ')
            << "\n";
    }
}

template<typename T>
void handleTypeSearch(const vector<char>& buffer)
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

vector<char> readFileBuffer(const string& prompt)
{
    while (true)
    {
        cout << prompt;

        string fileName;

        if (!getline(cin, fileName))
        {
            exit(1);  // EOF or terminal error
        }

        ifstream file(fileName, ios::binary);

        if (file)
        {
            // read whole file
            vector<char> buf((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

            return buf;
        }

        cerr << "Could not open file \"" << fileName << "\". Please try again.\n";
    }
}

int main()
{
    cout << "==== Resolution Bytes Finder v1.1 by AlphaYellow, 2025 ====\n\n";

    auto buffer = readFileBuffer("- Enter file name: ");

    char again = '2';

    do
    {
        // 1) choose data type
        enum TypeID { UINT8 = 1, INT16, UINT16, INT32, UINT32, FLOAT, DOUBLE };

        cout << "\n- Choose a data type:\n"
             << " 1) 8-bit unsigned integer  2) 16-bit signed integer    3) 16-bit unsigned integer" << endl
             << " 4) 32-bit signed integer   5) 32-bit unsigned integer  6) 32-bit floating point\n"
             << " 7) 64-bit floating point\n"
             << "Selection> ";
        
        int selectedType = readInput<int>(1, 7);

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

        case FLOAT:
            handleTypeSearch<float>(buffer);
            break;

        case DOUBLE:
            handleTypeSearch<double>(buffer);
            break;

        default:
            cout << "Unknown selection.\n";
            break;
        }

        // 3) ask to try again or exit
        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        
        again = readInput<char>('1', '2');
    } while (again == '2');

    cout << "\nPress Enter to exit...";

    cin.get();

    return 0;
}
