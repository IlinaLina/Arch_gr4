#include "Manager.h"
#include <iostream>
#include <string>

int main() {
    Manager manager(2);
    std::string abs_path = std::filesystem::current_path().string();

    while (true)
    {
        std::string choice;

        std::cout << "Enter the name of the folder with files to be processed (it should be in the same directory with script)" << std::endl;
        std::cout << "Or enter 'settings' or 'exit'" << std::endl;

        std::getline(std::cin, choice);

        if(choice == "settings") {
            std::cout << "Choose:" << std::endl;
            std::cout << "1) Set number of threads. Current value (" << manager.getProcessorNumber() << ")" << std::endl;
            std::cout << "2) Set slice size. Current value (" << manager.getSliceSize() << ")" << std::endl;
            std::cout << "3) Set blur strength. Current value (" << manager.getBlurStrength() << ")" << std::endl;
            std::cout << "Or enter smt else to back to menu" << std::endl;
            
            int num;
            std::cin >> num;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }

            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Очищаем после ввода числа

            if(num == 1) {
                int value;
                std::cout << "Enter new thread count: ";
                std::cin >> value;
                
                if (!std::cin.fail() && value > 0 && value < 16) {
                    manager.setProcessorNumber(value);
                    std::cout << "Thread count set to " << value << std::endl;
                } else {
                    std::cout << "Invalid value" << std::endl;
                    std::cin.clear();
                }
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
            else if(num == 2) {
                int value;
                std::cout << "Enter new slice size: ";
                std::cin >> value;
                
                if (!std::cin.fail() && value > 0) {
                    manager.setSliceSize(value);
                    std::cout << "Slice size set to " << value << std::endl;
                } else {
                    std::cout << "Invalid value" << std::endl;
                    std::cin.clear();
                }
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
            else if(num == 3) {
                int value;
                std::cout << "Enter new blur strength: ";
                std::cin >> value;
                
                if (!std::cin.fail() && value > 0) {
                    manager.setBlurStrength(value);
                    std::cout << "Blur strength set to " << value << std::endl;
                } else {
                    std::cout << "Invalid value" << std::endl;
                    std::cin.clear();
                }
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }

        else if (choice == "exit") {
            break;
        }

        else {
            auto img_path = abs_path + "/" + choice + "/";
            auto res_path = abs_path + "/results/";

            if (!fs::exists(img_path) || !fs::is_directory(img_path)) {
                std::cout << "Folder does not exist or is not a directory! Try again" << std::endl;
                continue;
            }

            std::cout << "Choose filter:" << std::endl;
            std::cout << "1) Blur" << std::endl;
            std::cout << "2) Invert" << std::endl;
            std::cout << "3) Contrast" << std::endl;
            std::cout << "4) All Blue" << std::endl;
            std::cout << "5) All Green" << std::endl;
            std::cout << "6) All Red" << std::endl;
            std::cout << "Or enter any other string to go back" << std::endl;
            std::cout << "Your choice: " << std::endl;

            int num;
            std::cin >> num;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }


            if(num == 1) {
                manager.runBlur(img_path, res_path);
            }

            else if(num == 2) {
                manager.run(img_path, res_path, invert_filter);
            }

            else if(num == 3) {
                manager.run(img_path, res_path, contrast_filter);
            }

            else if(num == 4) {
                manager.run(img_path, res_path, all_blue_filter);
            }

            else if(num == 5) {
                manager.run(img_path, res_path, all_green_filter);
            }

            else if(num == 6) {
                manager.run(img_path, res_path, all_red_filter);
            }

            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    return 0; 
}
