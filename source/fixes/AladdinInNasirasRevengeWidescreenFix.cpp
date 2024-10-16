#include <iostream>
#include <fstream>
#include <string>
#include <limits>
#include <vector>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <exception>
#include <span>
#include <string_view>
#include <concepts>
#include <array>
#include <iomanip>
#include <functional>

namespace AladdinWidescreenFixer
{

    // Constants for patterns and masks
    constexpr std::array<char, 10> aspectRatioPattern = {'\xFB', '\x4A', '\x00', '\x00', '\x00', '\x40', '\x3F', '\x0F', '\x94', '\xC2'};
    constexpr std::array<char, 10> aspectRatioMask = {'x', 'x', 'x', '?', '?', '?', '?', 'x', 'x', 'x'};
    constexpr std::array<char, 10> cameraFOVPattern = {'\xA6', '\x4A', '\x00', '\x86', '\xC8', '\x5A', '\x3F', '\xD8', '\x35', '\x60'};
    constexpr std::array<char, 10> cameraFOVMask = {'x', 'x', 'x', '?', '?', '?', '?', 'x', 'x', 'x'};

    // Constants for default values
    constexpr double defaultAspectRatio = 0.75;
    constexpr double defaultFOV = 0.8546222448;
    constexpr uint32_t minResolution = 1;
    constexpr uint32_t maxResolution = 65535;
    constexpr char filename[] = "aladdin.exe";

    // Exception classes
    class FileOpenException : public std::runtime_error
    {
    public:
        explicit FileOpenException(const std::string &msg) : std::runtime_error(msg) {}
    };

    class PatternNotFoundException : public std::runtime_error
    {
    public:
        explicit PatternNotFoundException(const std::string &msg) : std::runtime_error(msg) {}
    };

    // Utility functions
    template <typename T>
    concept Numeric = std::integral<T> || std::floating_point<T>;

    template <Numeric T>
    [[nodiscard]] T GetNumericInput(const std::string &prompt, T minValue, T maxValue)
    {
        T value;
        while (true)
        {
            std::cout << prompt;
            std::string input;
            std::getline(std::cin, input);
            try
            {
                if constexpr (std::is_integral_v<T>)
                {
                    value = static_cast<T>(std::stoll(input));
                }
                else
                {
                    // Replace commas with dots
                    std::replace(input.begin(), input.end(), ',', '.');
                    value = static_cast<T>(std::stold(input));
                }
                if (value >= minValue && value <= maxValue)
                {
                    break;
                }
                else
                {
                    std::cout << "Please enter a valid number between " << minValue << " and " << maxValue << ".\n";
                }
            }
            catch (const std::exception &)
            {
                std::cout << "Invalid input. Please enter a numeric value.\n";
            }
        }
        return value;
    }

    [[nodiscard]] int GetChoice(const std::string &prompt, const std::vector<std::string> &options)
    {
        int choice = 0;
        while (true)
        {
            std::cout << prompt;
            for (size_t i = 0; i < options.size(); ++i)
            {
                std::cout << "\n"
                          << (i + 1) << ". " << options[i];
            }
            std::cout << "\nEnter your choice: ";
            std::string input;
            std::getline(std::cin, input);
            try
            {
                choice = std::stoi(input);
                if (choice >= 1 && choice <= static_cast<int>(options.size()))
                {
                    break;
                }
                else
                {
                    std::cout << "Please enter a valid option number.\n";
                }
            }
            catch (const std::exception &)
            {
                std::cout << "Invalid input. Please enter a numeric value.\n";
            }
        }
        return choice;
    }

    // Function to open the file, retries if failed
    std::fstream OpenFile(const std::filesystem::path &filePath)
    {
        std::fstream file;
        while (true)
        {
            file.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
            if (file.is_open())
            {
                std::cout << "\n"
                          << filePath << " opened successfully!\n";
                return file;
            }
            else
            {
                std::cerr << "\nFailed to open " << filePath
                          << ". Please ensure the file exists, is not read-only, and is not currently running.\n";
                std::cout << "Press Enter to retry...";
                std::cin.get();
            }
        }
    }

    // Function to find a pattern in the file and return the position
    [[nodiscard]] std::optional<std::streampos> FindPatternInFile(std::fstream &file, std::span<const char> pattern, std::span<const char> mask)
    {
        file.seekg(0, std::ios::end);
        const auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(fileSize);
        if (!file.read(buffer.data(), fileSize))
        {
            std::cerr << "Error reading file.\n";
            return std::nullopt;
        }

        const size_t patternSize = mask.size();

        for (size_t i = 0; i <= fileSize - patternSize; ++i)
        {
            bool found = true;
            for (size_t j = 0; j < patternSize; ++j)
            {
                if (mask[j] == 'x' && buffer[i + j] != pattern[j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                // Find the first unknown byte
                for (size_t j = 0; j < patternSize; ++j)
                {
                    if (mask[j] == '?')
                    {
                        return static_cast<std::streampos>(i + j);
                    }
                }
            }
        }
        return std::nullopt;
    }

    // Function to write a float value to a pattern found in the file
    bool WriteValueToPattern(std::fstream &file, std::span<const char> pattern, std::span<const char> mask, float value)
    {
        auto offset = FindPatternInFile(file, pattern, mask);
        if (offset)
        {
            file.seekp(*offset);
            if (!file.write(reinterpret_cast<const char *>(&value), sizeof(value)))
            {
                std::cerr << "Error writing value to file at offset " << *offset << ".\n";
                return false;
            }
            return true;
        }
        else
        {
            std::cerr << "Pattern not found.\n";
            return false;
        }
    }

    // Function to calculate the new aspect ratio
    constexpr double CalculateAspectRatio(uint32_t width, uint32_t height)
    {
        return defaultAspectRatio / (static_cast<double>(width) / height / (4.0 / 3.0));
    }

    // Function to calculate the new field of view
    constexpr double CalculateFOV(uint32_t width, uint32_t height)
    {
        return defaultFOV / ((4.0 / 3.0) / (static_cast<double>(width) / height));
    }

    // Main processing function
    void RunWidescreenFixer()
    {
        std::cout << "Aladdin in Nasira's Revenge (2001) Widescreen Fixer v1.4 by AlphaYellow, 2024\n\n----------------\n";

        bool continueProgram = true;

        while (continueProgram)
        {
            // Get the desired width and height from the user
            uint32_t newWidth = GetNumericInput<uint32_t>("\n- Enter the desired width: ", minResolution, maxResolution);
            uint32_t newHeight = GetNumericInput<uint32_t>("\n- Enter the desired height: ", minResolution, maxResolution);

            const double newAspectRatio = CalculateAspectRatio(newWidth, newHeight);
            const float newAspectRatioAsFloat = static_cast<float>(newAspectRatio);

            // Open the file
            std::fstream file = OpenFile(filename);

            // Write the new aspect ratio value
            if (WriteValueToPattern(file, aspectRatioPattern, aspectRatioMask, newAspectRatioAsFloat))
            {
                std::cout << "Aspect ratio updated successfully.\n";
            }

            // Ask the user for FOV handling
            int choice = GetChoice("\n- Do you want to fix the FOV automatically based on the resolution typed above or set a custom FOV multiplier value?",
                                   {"Fix automatically", "Set custom value"});

            double newCameraFOV = 0.0;
            if (choice == 1)
            {
                newCameraFOV = CalculateFOV(newWidth, newHeight);
            }
            else if (choice == 2)
            {
                newCameraFOV = GetNumericInput<double>("\n- Enter a custom field of view multiplier value (default for 4:3 aspect ratio is 0.8546222448): ", 0.0, std::numeric_limits<double>::max());
            }

            const float newCameraFOVAsFloat = static_cast<float>(newCameraFOV);

            // Write the new camera FOV value
            if (WriteValueToPattern(file, cameraFOVPattern, cameraFOVMask, newCameraFOVAsFloat))
            {
                std::cout << "Camera FOV updated successfully.\n";
            }

            // Check for file errors
            if (file.good())
            {
                std::cout << "\nSuccessfully changed the aspect ratio and field of view.\n";
            }
            else
            {
                std::cout << "\nError(s) occurred during the file operations.\n";
            }

            file.close();

            // Ask the user if they want to continue
            int exitChoice = GetChoice("\n- Do you want to exit the program or try another value?", {"Exit", "Try another value"});
            if (exitChoice == 1)
            {
                continueProgram = false;
            }
            else
            {
                std::cout << "\n---------------------------\n";
            }
        }

        std::cout << "\nPress Enter to exit the program...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
    }

} // namespace AladdinWidescreenFixer

int main()
{
    AladdinWidescreenFixer::RunWidescreenFixer();
    return 0;
}
