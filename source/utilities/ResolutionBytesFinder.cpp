#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <conio.h>
#include <string>
#include <algorithm>

// Variables
std::string executableName;
int16_t firstValue, secondValue, byteRange, number;
int choice, tempChoice;
bool validKeyPressed;
char ch;

// Function to handle user input in choices
void HandleChoiceInput(int &choice)
{
    tempChoice = -1;         // Temporary variable to store the input
    validKeyPressed = false; // Flag to track if a valid key was pressed

    while (true)
    {
        ch = _getch(); // Waits for user to press a key

        // Checks if the key is '1' or '2'
        if ((ch == '1' || ch == '2') && !validKeyPressed)
        {
            tempChoice = ch - '0';  // Converts char to int and stores the input temporarily
            std::cout << ch;        // Echoes the valid input
            validKeyPressed = true; // Sets the flag as a valid key has been pressed
        }
        else if (ch == '\b' || ch == 127) // Handles backspace or delete keys
        {
            if (tempChoice != -1) // Checks if there is something to delete
            {
                tempChoice = -1;         // Resets the temporary choice
                std::cout << "\b \b";    // Erases the last character from the console
                validKeyPressed = false; // Resets the flag as the input has been deleted
            }
        }
        // If 'Enter' is pressed and a valid key has been pressed prior
        else if (ch == '\r' && validKeyPressed)
        {
            choice = tempChoice;    // Assigns the temporary input to the choice variable
            std::cout << std::endl; // Moves to a new line
            break;                  // Exits the loop since we have a confirmed input
        }
    }
}

// Function to handle user input in resolution
int16_t HandleNumberInput()
{
    do
    {
        std::cin >> number;

        cin.clear();                                         // Clears error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            std::cout << "Invalid input. Please enter a numeric value." << std::endl;
        }
        else if (number <= 0 || number >= 65535)
        {
            std::cout << "Please enter a valid number." << std::endl;
        }
    } while (number <= 0 || number > 65535);

    return number;
}

int main()
{
    do
    {
        std::cout << "- Enter the executable name: ";
        std::getline(std::cin, executableName);

        std::ifstream file(executableName, std::ios::binary);
        if (!file)
        {
            std::cerr << "\nCould not open the file." << std::endl;
            return 1;
        }

        std::cout << "\n- Enter the first 2-byte integer value: ";
        firstValue = HandleNumberInput();
        std::cout << "\n- Enter the second 2-byte integer value: ";
        secondValue = HandleNumberInput();
        std::cout << "\n- Enter the byte search range: ";
        byteRange = HandleNumberInput();

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

        std::cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            std::cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }

        std::cout << "\n";
    } while (choice != 1); // Checks the flag in the loop condition
}