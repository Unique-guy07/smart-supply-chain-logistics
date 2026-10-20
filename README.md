# Smart Supply Chain & Warehouse Logistics Platform (C++20)

An integrated, enterprise-scale logistics and warehouse management system implemented in modern C++20. This system serves as a capstone demonstration of practical Data Structures and Algorithms (DSA) applied to a realistic supply-chain environment.

Unlike isolated DSA textbook exercises, this platform establishes a continuous, interconnected business workflow connecting inventory indexing, order scheduling, dynamic pricing, shortest-path routing, 3D cargo container packing, guarded two-phase commit dispatch, automated guided vehicle (AGV) deadlock resolution, cyclic warehouse auditing, and dual-ended operational task management.

---

## 1. Project Overview & Business Workflow

The system models the operational life cycle of goods flowing through a distribution facility:

```
Inbound Inventory
       ↓
Multi-Tier Indexing (HashTable + LinearProbing + BST) & ABC Slotting
       ↓
Customer Order Intake & DEPQ Priority Scheduling (MinMaxHeap)
       ↓
Dynamic Freight Pricing (Shunting Yard + RPN Stack Evaluation)
       ↓
Warehouse Shortest-Path Route Planning (Sequential Multi-Leg Dijkstra)
       ↓
3D Physical Cargo Container Packing (Extreme Points & Bottom-Left-Fill)
       ↓
Guarded Two-Phase Commit Dispatch & Stock Deduction
       ↓
AGV Traffic Coordination & Deadlock Preemption (Wait-For Graph 3-Color DFS)
       ↓
Continuous Warehouse QA Auditing (Circular Linked List Josephus Elimination)
```

The system preserves transactional consistency between stages: prevalidations are strictly read-only and guarantee zero mutation, while guarded dispatch atomically verifies candidate identity and stock before executing order fulfillment.

---

## 2. Implemented Data Structures & System Roles

Every data structure in this repository fulfills a concrete operational requirement:

| Data Structure | Header | System Role & Module |
|---|---|---|
| **Separate-Chaining Hash Table** | `include/inventory/HashTable.h` | Primary master inventory index (O(1) average lookup for central catalog). |
| **Linear-Probing Hash Table** | `include/inventory/LinearProbingHashTable.h` | Secondary embedded/device inventory index (compact cache-friendly lookup). |
| **Binary Search Tree (BST)** | `include/inventory/BST.h` | Non-owning lexicographically ordered product index and ABC slotting. |
| **Min-Max Heap (DEPQ)** | `include/order/MinMaxHeap.h` | Dual-ended priority queue for customer order dispatch and task management. |
| **Queue (FIFO)** | `include/order/Queue.h` | Priority-lane order buffering and unweighted BFS graph traversal. |
| **Stack (LIFO)** | `include/order/Stack.h` | Shunting-Yard operator stack, RPN evaluation, and graph path backtracking. |
| **Circular Linked List** | `include/order/CircularLinkedList.h` | Warehouse QA inspection ring and cyclic Josephus elimination schedule. |
| **Binomial Heap** | `include/task/BinomialHeap.h` | Merging distributed priority queues across regional distribution centers. |
| **Fibonacci Heap** | `include/task/FibonacciHeap.h` | Advanced priority queue supporting theoretical O(1) amortized decrease-key. |
| **Min-Heap & Max-Heap** | `include/task/MinHeap.h`, `include/task/MaxHeap.h` | Prim's MST candidate edge selection and worker performance ranking. |
| **Disjoint-Set (Union-Find)** | `include/graph/DisjointSet.h` | Kruskal's MST infrastructure planning with path compression and union by rank. |
| **Adjacency List Graph** | `include/graph/WarehouseGraph.h` | Warehouse navigation network supporting bidirectional aisles and conveyors. |
| **Wait-For Graph (WFG)** | `include/graph/WaitForGraph.h` | Directed AGV blocking dependency representation for deadlock cycle detection. |
| **Extreme Points List & Grid** | `include/packing/CargoPacking.h` | Candidate 3D placement generation for discrete cargo container packing. |

---

## 3. Implemented Algorithms & System Roles

| Algorithm | Implementation File | System Role & Application |
|---|---|---|
| **Dijkstra Shortest Path** | `src/graph/WarehouseGraph.cpp` | Weighted picker and AGV transit optimization across warehouse aisles. |
| **Breadth-First Search (BFS)** | `src/graph/WarehouseGraph.cpp` | Unweighted minimum-hop route generation between storage locations. |
| **Kruskal's Algorithm** | `src/graph/WarehouseGraph.cpp` | Capital infrastructure connection planning (Minimum Spanning Forest). |
| **Prim's Algorithm** | `src/graph/WarehouseGraph.cpp` | Alternative Minimum Spanning Tree infrastructure planning using MinHeap. |
| **Three-Color DFS** | `src/graph/WaitForGraph.cpp` | AGV circular-wait deadlock detection (White/Gray/Black cycle extraction). |
| **Josephus Problem** | `src/order/AuditCycle.cpp` | Cyclic warehouse bin sampling and QA audit elimination schedules. |
| **Shunting-Yard Algorithm** | `src/order/ExpressionEvaluator.cpp` | Infix-to-postfix conversion for dynamic freight and shipping pricing expressions. |
| **Postfix / RPN Evaluation** | `src/order/ExpressionEvaluator.cpp` | Stack-based arithmetic evaluation of compiled freight pricing formulas. |
| **Pareto ABC Classification** | `src/inventory/SlottingEngine.cpp` | Hierarchical inventory classification based on cumulative asset valuation. |
| **Quick Sort** | `src/packing/CargoPacking.cpp` | Multi-key cargo sorting (descending volume, weight, lexicographic ID). |
| **Heap Sort** | `src/task/OperationalMetricsRanker.cpp` | Ranking warehouse worker throughput and operational KPIs using MaxHeap. |
| **Bottom-Left-Fill (BLF)** | `src/packing/CargoPacking.cpp` | Feasible 3D spatial container placement with support and center-of-mass checks. |

---

## 4. Architecture & Integration Facade (`SupplyChainPlatform`)

The application-level integration is coordinated by [`SupplyChainPlatform`](include/integration/SupplyChainPlatform.h).

```
                      +-----------------------+
                      |  SupplyChainPlatform  |
                      +-----------------------+
                                  |
    +---------------+-------------+-------------+---------------+
    |               |             |             |               |
    v               v             v             v               v
InventoryManager OrderManager WarehouseGraph WaitForGraph  AuditCycle / TaskManager
(Tier 1/2/3)     (DEPQ/Pricing) (Dijkstra/MST) (3-Color DFS) (Josephus / MinMaxHeap)
```

### Architectural Highlights
- **Strict Initialization Order**: Member declaration order guarantees that dependencies outlive dependent objects (`m_inventory` is initialized before `m_orderManager`, which binds a reference `InventoryManager&`).
- **Resource Safety (RAII)**: All classes manage memory through deterministic RAII and strict value/reference semantics. Copy and move constructors are deleted on the facade to prevent state tearing.
- **Read-Only Prevalidations**:
  - `planOrderRoute(...)`: Computes sequential multi-leg Dijkstra routes without mutating order or inventory state.
  - `validateOrderPacking(...)`: Expands ordered SKU quantities into discrete 3D `Package` units and validates container fit without mutating order or inventory state.
- **Guarded Order Dispatch**:
  - Peeks top candidate in DEPQ.
  - Verifies candidate is in `OrderStatus::Pending`.
  - Runs route and 3D cargo packing prevalidations.
  - Executes immediate pre-dispatch candidate identity recheck to ensure no intervening cancellation or preemption.
  - Executes single atomic fulfillment transaction via `OrderManager::dispatchHighestPriorityOrder()`.
  - Reports authoritative terminal status (`Completed` or `Failed`) and reflects exact inventory deductions.

---

## 5. Prerequisites & Build Instructions

### Prerequisites
- **Operating System**: Windows 10/11 (x64)
- **Compiler**: Visual Studio 2022 (MSVC v143+) with C++20 support
- **Build System**: CMake 3.20 or newer

### Building from Command Line (Developer PowerShell or CMD)

1. Clone or navigate to the repository:
   ```powershell
   cd "D:\VIT SYLLABUS\SY\Sem 3\DS\DS CP\smart-supply-chain-logistics"
   ```

2. Configure CMake in Debug mode:
   ```powershell
   cmake -B build -S .
   ```

3. Build the core library, main application, and test suite:
   ```powershell
   cmake --build build --config Debug --parallel 2
   ```

---

## 6. Running Tests

The test suite consists of 29 CTest suites containing hundreds of individual assertions:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

To run individual focused test suites directly:
```powershell
.\build\Debug\test_platform_prevalidation.exe
.\build\Debug\test_order_manager.exe
.\build\Debug\test_warehouse_graph.exe
.\build\Debug\test_deadlock.exe
.\build\Debug\test_cargo_packing.exe
```

---

## 7. Running the Application CLI

The primary executable is `smart_supply_chain.exe`.

### Automated / Non-Interactive Demonstration (`--demo`)
To run the automated 10-step end-to-end demonstration (ideal for grading, faculty reviews, or CI):
```powershell
.\build\Debug\smart_supply_chain.exe --demo
```

### Interactive Command-Line Interface
Launch the application without arguments to enter the interactive menu:
```powershell
.\build\Debug\smart_supply_chain.exe
```

```
================================================================================
  SMART SUPPLY CHAIN & WAREHOUSE LOGISTICS PLATFORM (C++20 DSA INTEGRATION)
================================================================================
Platform initialized with standard warehouse demonstration dataset.

------------------------------- OPERATIONAL MENU -------------------------------
  [1] Run Full End-to-End Logistics Demonstration (Automated / All Modules)
  [2] Guarded Order Dispatch & Route / 3D Cargo Packing Prevalidation
  [3] Inventory Catalog & Hierarchical ABC Slotting Analysis
  [4] AGV Traffic & Deadlock Detection / Resolution (Wait-For Graph)
  [5] Warehouse Cyclic QA Audit (Josephus Elimination Cycle)
  [6] Operational Task Prioritization (Dual-Ended MinMaxHeap)
  [7] Reset & Re-seed Platform State
  [0] Exit Application
--------------------------------------------------------------------------------
Select an option [0-7]:
```

### Interactive Features Walkthrough
- **Option 1**: Runs the complete 10-step demonstration touching all modules.
- **Option 2**: Performs read-only route planning, 3D cargo packing prevalidation, and guarded dispatch of the top pending order.
- **Option 3**: Inspects catalog items in lexicographical BST order, performs exact SKU lookups across Master and Device tiers, and outputs the Pareto ABC slotting report.
- **Option 4**: Detects AGV circular waits via 3-color DFS, displays the cycle chain, and performs deterministic victim selection based on priority.
- **Option 5**: Previews and executes cyclic QA audits on warehouse storage locations using Josephus elimination.
- **Option 6**: Manages emergency events and routine maintenance tasks via dual-ended MinMaxHeap dispatch.
- **Option 7**: Resets and re-seeds the platform with a clean demonstration state.

---

## 8. Expected Demonstration Output

Executing `.\build\Debug\smart_supply_chain.exe --demo` produces verified output covering all 10 steps:

```text
================================================================================
  RUNNING FULL END-TO-END SUPPLY CHAIN WORKFLOW DEMONSTRATION
================================================================================

[STEP 1] Warehouse Spatial Navigation Network (Modules 1, 2, 7)
  Topology initialized:
    - Operational Nodes: DEPOT, BIN_A1, BIN_B1, BIN_C1, PACK_STATION_1, DOCK_ALPHA
    - Bidirectional Aisles: DEPOT <-> BIN_A1 (12m), BIN_A1 <-> BIN_B1 (15m), BIN_B1 <-> BIN_C1 (20m)
                           BIN_A1 <-> PACK_STATION_1 (14m), BIN_B1 <-> PACK_STATION_1 (18m)
    - Directed Conveyor:   PACK_STATION_1 -> DOCK_ALPHA (8m)

[STEP 2] Multi-Tier Inventory Indexing & Physical Packaging Profiles (Modules 3, 5, 8)
  Indexed Catalog Items (Total SKUs: 5):
    * SKU-CABLE      | Name: Braided USB-C Cable 2m     | Qty: 200 | Price: $15.00   | Bin: BIN_B1         | Dim: 15x10x5cm (0.40kg)
    * SKU-DESK       | Name: Motorized Standing Desk    | Qty: 10  | Price: $600.00  | Bin: BIN_C1         | Dim: 120x80x20cm (35.00kg)
    * SKU-KEYBOARD   | Name: Mechanical Pro Keyboard    | Qty: 100 | Price: $80.00   | Bin: BIN_A1         | Dim: 45x15x4cm (1.20kg)
    * SKU-LAPTOP     | Name: Enterprise Laptop 15       | Qty: 50  | Price: $1200.00 | Bin: BIN_A1         | Dim: 35x25x5cm (2.50kg)
    * SKU-MONITOR    | Name: Ultra-HD 4K Display 27     | Qty: 30  | Price: $450.00  | Bin: BIN_B1         | Dim: 60x40x15cm (6.00kg)

[STEP 3] Hierarchical Inventory Slotting Analysis (Pareto ABC Classification - Module 5)
  Slotting Report Overview:
    - Total Inventory Value: $90500.00
    - Class A Count: 2 | Class B Count: 1 | Class C Count: 2
    - Mis-slotted SKU Count: 5

[STEP 4] Inbound Order Intake & Dynamic Freight Pricing (Module 4)
  Pending Orders in Queue: 3
    * ORD-1001 (Priority: Urgent, Customer: CORP-GLOBAL, Shipping: 15.00km / 3.70kg) -> Freight: $40.92
    * ORD-1002 (Priority: Normal, Customer: CUST-RETAIL, Shipping: 25.00km / 12.00kg) -> Freight: $32.10
    * ORD-1003 (Priority: Low, Customer: STORE-SOUTH, Shipping: 40.00km / 35.00kg) -> Freight: $46.40
  DEPQ Priority Peeking: Highest-Priority Candidate = ORD-1001

[STEP 5] Read-Only Order Route Prevalidation (Module 7)
  Planning optimal picker route for candidate ORD-1001:
  Route Planning Succeeded! Total Distance: 34.00m across 3 sequential legs:
    Leg 1: DEPOT -> BIN_A1 (Cost: 12.00m, Waypoints: DEPOT -> BIN_A1)
    Leg 2: BIN_A1 -> PACK_STATION_1 (Cost: 14.00m, Waypoints: BIN_A1 -> PACK_STATION_1)
    Leg 3: PACK_STATION_1 -> DOCK_ALPHA (Cost: 8.00m, Waypoints: PACK_STATION_1 -> DOCK_ALPHA)

[STEP 6] Read-Only 3D Cargo Packing Prevalidation (Module 8)
  Target Container Dimensions: 120 x 80 x 80 cm (Volume: 768000 cm3)
  Cargo Packing Status: FEASIBLE (All Placed)
  Summary: Packed 2/2 packages (0 unplaced). Total weight: 3.700000. Total volume: 7075/768000.
  Placed Packages: 2 | Volume Utilization: 0.9%

[STEP 7] Guarded Order Dispatch & Fulfillment Transaction (Workflow Integration)
  Pre-dispatch Inventory Stock: SKU-LAPTOP = 50, SKU-KEYBOARD = 100
  Dispatch Execution Outcome:
    - Dispatched Order ID: ORD-1001
    - Dispatch Attempted:   YES
    - Fulfillment Success:  YES
    - Resulting Status:     Completed
    - Diagnostic Message:   SupplyChainPlatform::dispatchHighestPriorityOrder: order ORD-1001 successfully dispatched and fulfilled
  Post-dispatch Inventory Stock: SKU-LAPTOP = 49 (change: -1), SKU-KEYBOARD = 99 (change: -1)
  Remaining Pending Orders in Queue: 2

[STEP 8] AGV Traffic Coordination & Deadlock Cycle Resolution (Module 7)
  Deadlock Detection Query: DEADLOCK DETECTED!
  Circular Wait Dependency Chain: AGV-ALPHA -> AGV-BETA -> AGV-GAMMA -> AGV-ALPHA
  Deterministic Victim Selection: AGV-GAMMA (Selected by lowest TaskPriority: Low, preserving higher-priority traffic)
  Post-Resolution Verification: RESOLVED (Cycle Cleared, Traffic Resumed)

[STEP 9] Warehouse Cyclic QA Auditing (Circular Linked List & Josephus - Module 4)
  Active Audit Targets in Cycle: 5 locations
  Executing QA Audit Selection (Interval k = 3, Quota = 2 targets):
    * Selected for Audit Inspection: BIN_C1, BIN_A1
  Remaining Active Audit Targets: 3 locations

[STEP 10] Operational Task Prioritization (Dual-Ended MinMaxHeap - Module 6)
  Active Tasks in Coordinator Queue: 4
  Dispatched Highest Priority (Emergency): TASK-SPILL [Critical] Category: Emergency
  Dispatched Lowest Priority (Deferrable): TASK-SWEEP [Low] Category: Maintenance
  Remaining Active Tasks in Coordinator Queue: 2

================================================================================
  [SUCCESS] ALL INTEGRATED MODULES DEMONSTRATED WITH 100% PASSING INVARIANTS
================================================================================
```

---

## 9. Known Limitations

1. **Single-Threaded Execution**: The guarded dispatch mechanism assumes a single-threaded workflow without concurrent threads modifying order or inventory state between prevalidation recheck and dispatch.
2. **Lazy Tombstone Invalidation**: In `OrderManager` and `TaskPriorityManager`, cancelled items are marked as cancelled in the registry and lazily purged from heaps upon root peeking/extraction.
3. **In-Memory Volatility**: The platform operates in-memory; restart resets platform state unless seeded.

---

## 10. Specification Compliance & Gap Analysis

In accordance with strict verification standards, the following differences between theoretical design sketches in `PROJECT_SPEC.md` and the final implementation are documented:

- **Module 1 (Spatial Representation)**: The specification originally outlined a 2D contiguous array, a 3-tuple sparse matrix, and a fast transpose algorithm for warehouse floor plans. In the operational implementation, spatial navigation and routing were standardized on the graph adjacency network (`WarehouseGraph`), which directly drives BFS and Dijkstra route planning. The 2D contiguous array, 3-tuple sparse matrix, and fast transpose remain unimplemented specification requirements.
- **Module 2 (Memory Management & Container Gaps)**: The specification described a custom pool allocator, a free list, and a doubly linked list. The production codebase adopts standard C++20 allocators with RAII value and move semantics, ensuring strong memory safety, exception safety, and preventing memory leaks without raw-pointer dangling risks. The custom pool allocator, free list, and doubly linked list remain unimplemented specification requirements.
- **Operational Scope vs. Specification Compliance**: While full specification compliance is not claimed due to the Module 1 and Module 2 gaps noted above, the operational logistics pipeline successfully integrates its planned functional workflow areas (Multi-Tier Inventory Indexing, DEPQ Order Scheduling & Pricing, Pareto Slotting, Dual-Ended Task Prioritization, Warehouse Shortest-Path Routing, AGV Deadlock Resolution, 3D Cargo Packing Prevalidation, and Guarded Platform Dispatch). All implemented operational components are verified across 29 automated test suites and fully exercisable through the CLI.
