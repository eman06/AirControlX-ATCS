# AirControlX-ATCS
C/C++ based real-time simulation of an Automated Air Traffic Control System for Operating Systems course at COMSATS. Implements core OS concepts like process management, threading, synchronization, scheduling, and inter-process communication.

Features
Dynamic Aircraft Simulation
Spawns and manages various types of aircraft (domestic, international, cargo, emergency) with individual properties like speed, altitude, and priority.

Runway Allocation & Management
Allocates runways in real-time based on aircraft type, availability, and urgency using efficient scheduling algorithms.

Violation Detection System
Monitors aircraft speed, airspace regulations, and runway compliance to detect violations. Each violation is recorded with a unique AVN (Aviation Violation Number).

Airline Portal & Violation Resolution
Simulated portals for airline representatives to view, appeal, or pay AVNs. Communication between airlines and the control system is achieved using IPC (pipes and shared memory).

Graphical Visualization
Real-time 2D simulation of aircraft movement, runway status, and alert notifications using SFML or OpenGL. Includes dashboards for flight monitoring and violation tracking.

Payment Gateway Simulation
Mimics a financial transaction system for airlines to resolve AVNs with confirmation, logging, and status updates.

🧠 Technologies Used
Core Language: C++

Concurrency: POSIX Threads (pthreads), mutexes, semaphores

Process Management: fork(), exec(), wait(), IPC (pipes/shared memory)

Graphics & Visualization: SFML / OpenGL

File Handling: Log generation, AVN records, airline history

⚙️ Key OS Concepts Implemented
Multithreading and synchronization

Inter-process communication

Process creation and management

Deadlock prevention and priority handling

Real-time I/O and graphical rendering
