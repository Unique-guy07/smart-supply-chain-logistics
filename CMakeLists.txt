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
    src/inventory/SlottingEngine.cpp
    src/order/Order.cpp
    src/order/OrderManager.cpp
    src/order/ExpressionEvaluator.cpp
    src/order/FreightPricingEngine.cpp
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

add_executable(test_queue tests/test_queue.cpp)
target_link_libraries(test_queue PRIVATE inventory_core)
add_test(NAME TestQueue COMMAND test_queue)

add_executable(test_order tests/test_order.cpp)
target_link_libraries(test_order PRIVATE inventory_core)
add_test(NAME TestOrder COMMAND test_order)

add_executable(test_order_manager tests/test_order_manager.cpp)
target_link_libraries(test_order_manager PRIVATE inventory_core)
add_test(NAME TestOrderManager COMMAND test_order_manager)

add_executable(test_min_max_heap tests/test_min_max_heap.cpp)
target_link_libraries(test_min_max_heap PRIVATE inventory_core)
add_test(NAME TestMinMaxHeap COMMAND test_min_max_heap)

add_executable(test_stack tests/test_stack.cpp)
target_link_libraries(test_stack PRIVATE inventory_core)
add_test(NAME TestStack COMMAND test_stack)

add_executable(test_expression_evaluator tests/test_expression_evaluator.cpp)
target_link_libraries(test_expression_evaluator PRIVATE inventory_core)
add_test(NAME TestExpressionEvaluator COMMAND test_expression_evaluator)

add_executable(test_freight_pricing tests/test_freight_pricing.cpp)
target_link_libraries(test_freight_pricing PRIVATE inventory_core)
add_test(NAME TestFreightPricing COMMAND test_freight_pricing)

add_executable(test_slotting tests/test_slotting.cpp)
target_link_libraries(test_slotting PRIVATE inventory_core)
add_test(NAME TestSlotting COMMAND test_slotting)