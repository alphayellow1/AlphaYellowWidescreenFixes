#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <limits>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <conio.h>
#include <string>
#include <algorithm>

using namespace std;

// Variables
string fileName;
int16_t firstValue, secondValue, byteRange, number;
int choice, tempChoice;
bool validKeyPressed, foundSecondValue;
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
            cout << ch;             // Echoes the valid input
            validKeyPressed = true; // Sets the flag as a valid key has been pressed
        }
        else if (ch == '\b' || ch == 127) // Handles backspace or delete keys
        {
            if (tempChoice != -1) // Checks if there is something to delete
            {
                tempChoice = -1;         // Resets the temporary choice
                cout << "\b \b";         // Erases the last character from the console
                validKeyPressed = false; // Resets the flag as the input has been deleted
            }
        }
        // If 'Enter' is pressed and a valid key has been pressed prior
        else if (ch == '\r' && validKeyPressed)
        {
            choice = tempChoice; // Assigns the temporary input to the choice variable
            cout << endl;        // Moves to a new line
            break;               // Exits the loop since we have a confirmed input
        }
    }
}

// Function to handle user input in resolution
int16_t HandleNumberInput()
{
    do
    {
        cin >> number;

        cin.clear();                                         // Clears error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input

        if (cin.fail())
        {
            cin.clear();                                         // Clears error flags
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignores invalid input
            cout << "Invalid input. Please enter a numeric value." << endl;
        }
        else if (number <= 0 || number >= 65535)
        {
            cout << "Please enter a valid number." << endl;
        }
    } while (number <= 0 || number > 65535);

    return number;
}

int main()
{
    cout << "\n- Enter the file name: ";
    getline(cin, fileName);

    do
    {
        ifstream file(fileName, ios::binary);
        if (!file)
        {
            cerr << "\nCould not open the file." << endl;
            return 1;
        }

        cout << "\n- Enter the first 2-byte integer value: ";
        firstValue = HandleNumberInput();
        cout << "\n- Enter the second 2-byte integer value: ";
        secondValue = HandleNumberInput();
        cout << "\n- Enter the byte search range: ";
        byteRange = HandleNumberInput();
        cout << "\n";

        vector<size_t> foundAddresses; // Vector to store the addresses where the second value is found

        vector<char> buffer(istreambuf_iterator<char>(file), {});
        for (size_t i = 0; i < buffer.size(); ++i)
        {
            if (i <= buffer.size() - 2 && *reinterpret_cast<int16_t *>(&buffer[i]) == firstValue)
            {
                for (int j = -byteRange; j <= byteRange; ++j)
                {
                    if (i + j >= 0 && i + j < buffer.size() - 2)
                    {
                        if (*reinterpret_cast<int16_t *>(&buffer[i + j]) == secondValue)
                        {
                            foundAddresses.push_back(i + j); // Store the address
                        }
                    }
                }
            }
        }

        if (!foundAddresses.empty())
        { // Check if any addresses were found
            cout << "Found second value (" << dec << secondValue << ") within " << dec << byteRange << " bytes of the first value (" << firstValue << ") at the following addresses:" << endl;
            for (const auto &address : foundAddresses)
            {
                cout << uppercase << setfill('0') << setw(8) << hex << address << endl;
            }
        }
        else
        {
            cout << "No second value (" << dec << secondValue << ") was found within " << dec << byteRange << " bytes of the first value (" << firstValue << ")." << endl;
        }

        file.close();

        cout << "\n- Do you want to exit the program (1) or try another value (2)?: ";
        HandleChoiceInput(choice);

        if (choice == 1)
        {
            cout << "\nPress enter to exit the program...";
            do
            {
                ch = _getch(); // Waits for user to press a key
            } while (ch != '\r'); // Keeps waiting if the key is not Enter ('\r' is the Enter key in ASCII)
            return 0;
        }
    } while (choice != 1); // Checks the flag in the loop condition
}