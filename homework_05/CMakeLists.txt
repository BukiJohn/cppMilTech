cmake_minimum_required(VERSION 3.20)

# Окремий CMake-проєкт для домашньої роботи 05. Його можна збирати як
# через root CMakeLists.txt, так і напряму з цієї директорії.
project(telemetry_analyzer CXX)

# Той самий стандарт, що й у решті курсу.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Бібліотека telemetry містить логіку читання, парсингу, валідації та підсумку.
add_library(telemetry src/telemetry.cpp)

# include/ підключаємо як PUBLIC: і сама бібліотека, і все, що з нею лінкується,
# бачать telemetry.hpp без додаткового налаштування.
target_include_directories(telemetry PUBLIC include)
target_compile_options(telemetry PRIVATE -Wall -Wextra -pedantic)

# Виконуваний файл telemetry_check - обгортка над бібліотекою. Він читає
# шлях з argv[1] і друкує підсумок.
add_executable(telemetry_check src/main.cpp)

# Залежність від telemetry: лінкування підтягує і код бібліотеки, і її
# PUBLIC include-каталог.
target_link_libraries(telemetry_check PRIVATE telemetry)
target_compile_options(telemetry_check PRIVATE -Wall -Wextra -pedantic)
