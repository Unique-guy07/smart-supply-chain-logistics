cmake_minimum_required(VERSION 4.2)

project(SmartSupplyChainLogistics
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/EHsc)
endif()

# ---------------------------------------------------------------------------
# Reusable inventory library
# ---------------------------------------------------------------------------
add_library(inventory_core STATIC
    src/inventory/Product.cpp
    src/inventory/HashTable.cpp
    src/inventory/LinearProbingHashTable.cpp
    src/inventory/BST.cpp
    src/inventory/InventoryManager.cpp
)
target_include_directories(inventory_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(smart_supply_chain src/main.cpp)
target_link_libraries(smart_supply_chain PRIVATE inventory_core)

# ---------------------------------------------------------------------------
# Tests (CTest)
# ---------------------------------------------------------------------------
enable_testing()

add_executable(test_product tests/test_product.cpp)
target_link_libraries(test_product PRIVATE inventory_core)
add_test(NAME TestProduct COMMAND test_product)

add_executable(test_hash_table tests/test_hash_table.cpp)
target_link_libraries(test_hash_table PRIVATE inventory_core)
add_test(NAME TestHashTable COMMAND test_hash_table)

add_executable(test_linear_probing_hash_table tests/test_linear_probing_hash_table.cpp)
target_link_libraries(test_linear_probing_hash_table PRIVATE inventory_core)
add_test(NAME TestLinearProbingHashTable COMMAND test_linear_probing_hash_table)

add_executable(test_bst tests/test_bst.cpp)
target_link_libraries(test_bst PRIVATE inventory_core)
add_test(NAME TestBST COMMAND test_bst)

add_executable(test_inventory_manager tests/test_inventory_manager.cpp)
target_link_libraries(test_inventory_manager PRIVATE inventory_core)
add_test(NAME TestInventoryManager COMMAND test_inventory_manager)
