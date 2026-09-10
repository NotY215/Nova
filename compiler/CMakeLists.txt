add_library(nova_compiler STATIC
    lexer/Token.cpp
    lexer/Lexer.cpp
)
target_include_directories(nova_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(novac driver/main.cpp)
target_link_libraries(novac PRIVATE nova_compiler)