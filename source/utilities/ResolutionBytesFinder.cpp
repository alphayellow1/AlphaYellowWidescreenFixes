#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <conio.h>

int main()
{
    std::string executableName;
    std::cout << "- Enter the executable name: ";
    std::getline(std::cin, executableName);

    std::ifstream file(executableName, std::ios::binary);
    if (!file)
    {
        std::cerr << "\nCould not open the file." << std::endl;
        return 1;
    }

    int16_t firstValue, secondValue, byteRange;
    std::cout << "\n- Enter the first 2-byte integer value: ";
    std::cin >> firstValue;
    std::cout << "\n- Enter the second 2-byte integer value: ";
    std::cin >> secondValue;
    std::cout << "\n- Enter the byte search range: ";
    std::cin >> byteRange;

    std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        if (i <= buffer.size() - 2 && *reinterpret_cast<int16_t *>(&buffer[i]) == firstValue)
        {
            std::cout << "Found first value at address: " << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << i << std::endl;
            for (int j = -byteRange; j <= byteRange; ++j)
            {
                if (i + j >= 0 && i + j < buffer.size() - 2)
                {
                    if (*reinterpret_cast<int16_t *>(&buffer[i + j]) == secondValue)
                    {
                        std::cout << "Found second value within " << byteRange << " bytes of first value at address: " << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << i + j << std::endl;
                    }
                }
            }
        }
    }

    file.close();

    std::cout << "\nPress Enter to exit the program...";

    getch();

    return 0;
}