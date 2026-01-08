cmake_minimum_required(VERSION 3.16)
project(riskEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/instrument.cpp
    src/visitor.cpp
    src/model.cpp
)

# Create executable
add_executable(riskEngine ${SOURCES})
