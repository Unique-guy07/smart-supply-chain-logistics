\# Smart Supply Chain Logistics Platform — Agent Rules



\## 1. Source of Truth



`PROJECT\_SPEC.md` is the primary architectural specification for this project.



Before implementing a feature:

\- Read the relevant section of `PROJECT\_SPEC.md`.

\- Preserve the architecture defined there.

\- Do not replace planned components with simpler alternatives without explicit approval.



\## 2. Project Goal



This is an integrated Smart Supply Chain and Warehouse Logistics Platform.



The system must connect its components through a meaningful business workflow:



Inventory

→ Customer Order

→ Order Processing

→ Warehouse

→ Route Planning

→ Shipment

→ Delivery

→ Inventory Update



Do not implement unrelated standalone DSA demonstrations.



\## 3. Data Structures



Every data structure must have a genuine role in the system.



The project intentionally includes both foundational and advanced structures from the research architecture.



Do NOT remove a data structure merely because it is difficult to implement.



If a component is implemented later, preserve its planned architectural role.



\## 4. Implementation Discipline



Before modifying files:

1\. Inspect the existing repository.

2\. Understand the relevant architecture.

3\. Identify dependencies.

4\. Implement only the requested milestone.

5\. Build the project.

6\. Run relevant tests.

7\. Report what changed and what was verified.



Do not rewrite unrelated modules.



\## 5. C++ Standards



\- Use C++20.

\- Build using CMake.

\- Use the MSVC toolchain.

\- Prefer clear, modular C++.

\- Avoid unnecessary global state.

\- Avoid unexplained magic numbers.

\- Use `std::` explicitly rather than `using namespace std;` in project source/header files.



\## 6. Architecture



Keep responsibilities separated.



Prefer:



Headers:

`include/`



Implementations:

`src/`



Tests:

`tests/`



Documentation:

`docs/`



Data:

`data/`



Do not place the entire application in `main.cpp`.



`main.cpp` should eventually act primarily as the application entry point.



\## 7. Data Structure Implementation



When a custom data structure is part of the architecture, implement the relevant structure rather than silently replacing it with an STL container.



STL may be used where it does not replace a DSA that the project is specifically intended to demonstrate.



\## 8. Correctness



Do not claim that an implementation works without building and testing it.



For algorithms and data structures:

\- Verify edge cases.

\- Consider empty structures.

\- Consider duplicate data.

\- Consider invalid input.

\- Consider memory ownership and lifetime.

\- Consider synchronization between indexes.



\## 9. Memory Safety



Pay particular attention to:

\- raw pointers used by custom data structures

\- ownership

\- dynamic allocation

\- destructors

\- dangling pointers

\- double deletion

\- memory leaks

\- tree deletion

\- linked-list deletion



Use appropriate sanitization/debugging tools when practical.



\## 10. Testing



Every substantial module should have tests.



Tests should verify behavior, not merely compilation.



When a bug is discovered:

1\. Reproduce it.

2\. Identify the cause.

3\. Fix it.

4\. Add or update a regression test.

5\. Rebuild and rerun tests.



\## 11. Git Discipline



Keep commits focused and meaningful.



Do not make large unrelated commits.



A milestone should be committed only after:

\- implementation

\- build

\- tests

\- basic verification



\## 12. Agent Behavior



Agents must not:

\- redesign the project without approval

\- remove planned architecture because it is difficult

\- silently simplify algorithms

\- modify unrelated files

\- overwrite working code unnecessarily

\- fabricate test results

\- claim completion without verification



When an architectural conflict is discovered, stop and report it instead of making an arbitrary decision.



\## 13. Reporting



After completing a task, report:



1\. Files created or modified

2\. What was implemented

3\. Tests performed

4\. Build result

5\. Any known limitations

6\. Recommended next milestone



Keep reports concise but technically precise.

