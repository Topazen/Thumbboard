#include <iostream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
int main() {
    std::vector<std::vector<char>> example_grid = {
    {'Q', 'W', 'E'},
    {'A', 'S', 'D'},
    {'Z', 'X', 'C'}
    }; // how can out programm understand all these vectors?

int x = 1;
int y = 1;

const char* device_path = "dev/input/event14"; 
int  fd = open(device_path, O_RDONLY);

if (fd == -1){
    std::cerr << "Error\n";
return 1;
}
char user_input;
//bool is_running = true;
while (true) {
    std::cout <<"Letter  " << example_grid[y][x]<< "\n"; //What does \n does
    std::cout <<"Direction?: ";
    std::cin >> user_input;
    if (user_input == 'd' && x < 2){
        x = x + 1;
}
if (user_input == 'a' && x > 0){
        x = x - 1;
}
if (user_input == 's' && y < 2){
        y = y + 1;
}
if (user_input == 'w' && y > 0){
        y = y - 1;
}
//if (!(std::cin >> user_input)){
//    break;
//}
}
return 0;
}