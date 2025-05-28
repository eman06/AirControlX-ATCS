 /*
    🧚✈️ AIR TRAFFIC CONTROL SIMULATOR ✈️🧚
       BY EMAN IHSAN AND FATIMA TUZ ZAHRA



--------------------------------- MOD1 ----------------------------------
Aircraft Movement & Speed Violation Monitoring
- Each aircraft (thread) simulates a full journey: either ARRIVAL or DEPARTURE.
- ARRIVAL aircraft pass through: HOLDING → APPROACH → LANDING → TAXI → GATE.
- DEPARTURE aircraft pass through: GATE → TAXI → TAKEOFF → CLIMB → CRUISE.
- Speed is randomized in each phase (smtimes outside limits).
- A parallel radar monitor thread checks speed violations per aircraft per phase.
- AVNS are once per aircraft for any speed issue.
- Aircraft behavior is thread safe using per-aircraft mutex locks.

--------------------------------- MOD2 ----------------------------------
Runway Synchronization & Priority-Based Access
- Three runways (RWY-A, RWY-B, RWY-C) are shared resources protected by mutexes.
- Only one aircraft may use a runway at a time; access is synchronized.
- Emergency flights are given immediate access to runways( high priority using queue).
- Commercial and Cargo aircraft retry access based on increasing wait times.
- Aircraft requesting LANDING (ARRIVAL) or TAKEOFF (DEPARTURE) phases trigger runway requests.
- Once granted access, aircraft occupy the runway for a fixed time 3s, then release it.
- A global mutex (runway_queue_lock) ensures fair and race-free assignment decisions.
extra stuff:
- Multithreading: Each aircraft is a thread, simulating independent behavior.
- Radar Monitor: A monitoring thread operates like an air traffic radar system.
- Synchronization: Aircraft and runway threads require tight mutex coordination.
- Prioritization: Real-world emergency protocols influence access logic.
- Scalability: Design sets up for Module 3 (e.g., billing, GUI integration).
============================================================================
*/

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include<cmath>
using namespace std;
#define FINE_COMMERCIAL 5000
#define FINE_CARGO 3000
#define FINE_EMERGENCY 1000

// ========================== ENUMS AND CONSTANTS =============================

// Defines all the flight phases, aircraft types, and direction types

enum Phase { HOLDING, APPROACH, LANDING, TAXI, GATE, TAKEOFF, CLIMB, CRUISE };
enum FlightType { ARRIVAL, DEPARTURE };
enum AircraftType { COMMERCIAL, CARGO, EMERGENCY };
bool runwaysInUse[3] = {false, false, false}; // RWY-A, RWY-B, RWY-C

// Names of phases used in console output
const char* PHASE_NAMES[] = {
    "Holding", "Approach", "Landing", "Taxi", "Gate",
    "Takeoff", "Climb", "Cruise"
};
// Horizontal positions for each phase
const float PHASE_X_POSITIONS[] = {
    100.f, // HOLDING or GATE
    200.f, // APPROACH or TAXI
    300.f, // LANDING or TAKEOFF
    400.f, // TAXI or CLIMB
    500.f, // GATE or CRUISE
};


// Speed ranges per phase for ARRIVAL flights
const int ARRIVAL_SPEED_LIMITS[][2] = {
    {400, 600}, // Holding
    {240, 290}, // Approach
    {30, 240},  // Landing
    {15, 30},   // Taxi
    {0, 5}      // Gate
};

// Speed ranges per phase for DEPARTURE flights
const int DEPARTURE_SPEED_LIMITS[][2] = {
    {0, 5},     // Gate
    {15, 30},   // Taxi
    {0, 290},   // Takeoff
    {250, 463}, // Climb
    {800, 900}  // Cruise
};

const int NUM_AIRCRAFTS = 6; // Simulating 6 flights across airlines

sf::Texture commercialTexture;
    sf::Texture cargoTexture;
    sf::Texture emergencyTexture;
    sf::Texture runwayTexture;

// ========================== STRUCT DEFINITIONS ==============================

// Aircraft holds flight metadata, current phase, speed, and AVN tracking
struct Aircraft {
    char flight_number[10];
    FlightType direction;
    AircraftType type;
    Phase current_phase;
    int speed;
    bool is_active;
    bool avn_issued;
    pthread_mutex_t lock;
     sf::Vector2f position;  // For rendering
};
// Runway structure will be useful in Module 2
struct Runway {
    const char* name;
    pthread_mutex_t lock;
};

// ========================== GLOBAL VARIABLES ================================

Aircraft aircrafts[NUM_AIRCRAFTS];
Runway runways[3] = {
    {"RWY-A", PTHREAD_MUTEX_INITIALIZER},
    {"RWY-B", PTHREAD_MUTEX_INITIALIZER},
    {"RWY-C", PTHREAD_MUTEX_INITIALIZER}
};
bool simulation_running = true;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex for clean console output
// ========================== GLOBAL VARIABLES ================================
pthread_mutex_t runway_queue_lock = PTHREAD_MUTEX_INITIALIZER; // [MODULE 2]
pthread_cond_t runway_available = PTHREAD_COND_INITIALIZER;    // [MODULE 2]

// Priority values: lower means higher priority
int get_priority(AircraftType type) {
    int result;

if (type == EMERGENCY) {
    result = 0;
} else if (type == COMMERCIAL) {
    result = 1;
} else if (type == CARGO) {
    result = 2;
} else {
    result = 3;
}

return result;

}

// ========================== HELPER FUNCTIONS ================================
#define RESET_COLOR "\033[0m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define YELLOW_COLOR "\033[33m"
#define BLUE_COLOR "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define CYAN_COLOR "\033[36m"
#define WHITE_COLOR "\033[37m"
//pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
void safe_print(const string& msg) {
    pthread_mutex_lock(&print_lock);

    // Match patterns to assign colors
    if (msg.find("[Runway Assigned]") != string::npos) {
        cout << BLUE_COLOR << msg << RESET_COLOR << endl << endl;
    } else if (msg.find("[Runway Released]") != string::npos) {
        cout << MAGENTA_COLOR << msg << RESET_COLOR << endl << endl;
    } else if (msg.find("[AVN]") != string::npos) {
        cout << RED_COLOR << msg << RESET_COLOR << endl << endl;
    } else if (msg.find("[Flight ") != string::npos) {
        cout << GREEN_COLOR << msg << RESET_COLOR << endl;
    } else if (msg.find("Simulation Time Ended") != string::npos) {
        cout << YELLOW_COLOR << msg << RESET_COLOR << endl << endl;
    } else {
        cout << WHITE_COLOR << msg << RESET_COLOR << endl;
    }

    pthread_mutex_unlock(&print_lock);
}

void issue_avn(Aircraft* aircraft, const char* reason) {
    if (!aircraft->avn_issued) {
        int fine = (aircraft->type == COMMERCIAL) ? FINE_COMMERCIAL :
                   (aircraft->type == CARGO) ? FINE_CARGO : FINE_EMERGENCY;

        string log = "[AVN] Violation by Flight " + string(aircraft->flight_number) +
                     " - " + reason + " - Fine: $" + to_string(fine);
        safe_print(log);

        // Log to file
        FILE* f = fopen("avn_log.txt", "a");
        if (f) {
        time_t current_time = time(0);
            char* dt = ctime(&current_time);
            dt[strlen(dt) - 1] = '\0';  // Remove newline from ctime output
cout<<RED_COLOR;
            fprintf(f, "%s - %s - %s - Fine: $%d\n",
                    aircraft->flight_number,
                    (aircraft->type == COMMERCIAL ? "COMMERCIAL" :
                     aircraft->type == CARGO ? "CARGO" : "EMERGENCY"),
                    reason, fine);
            fclose(f);
        }

        aircraft->avn_issued = true;
    }
}

// [MODULE 2] Request a runway with priority-based access
Runway* request_runway(const char* flight_number, AircraftType type) {
    int my_priority = get_priority(type);

    while (simulation_running) {
        pthread_mutex_lock(&runway_queue_lock);
        for (int i = 0; i < 3; ++i) {
            if (!runwaysInUse[i] && pthread_mutex_trylock(&runways[i].lock) == 0) {
                runwaysInUse[i] = true;
                safe_print("[Runway Assigned] " + string(flight_number) + " is using " + runways[i].name);
                pthread_mutex_unlock(&runway_queue_lock);
                return &runways[i];
            }
        }

        // If no runway is free, emergency flights wait less
        int wait_time;

if (my_priority == 0) {
    wait_time = 1;
} else {
    wait_time = 2 + my_priority;
}

        pthread_mutex_unlock(&runway_queue_lock);
        sleep(wait_time); // Simulate retry delay based on priority
    }
    return nullptr;
}


void release_runway(Runway* runway) {
    runwaysInUse[runway - &runways[0]] = false; // Free up the runway
    pthread_mutex_unlock(&runway->lock); // Unlock the runway
   
    safe_print("[Runway Released] Runway " + string(runway->name) + " is now available.");
}



// ========================== THREAD FUNCTIONS ================================

/*
Each aircraft thread runs this function. It assigns the correct
sequence of phases based on arrival or departure type, then simulates
speed in each phase. Speeds are randomized within acceptable ranges,
but may occasionally violate rules, allowing radar to trigger AVNs.
*/


// Helper function to request a runway


void* flight_simulation(void* arg) {
    Aircraft* aircraft = (Aircraft*)arg;
    Phase phases[5];
    int num_phases = 0;

    if (aircraft->direction == ARRIVAL) {
        Phase temp[] = { HOLDING, APPROACH, LANDING, TAXI, GATE };
        memcpy(phases, temp, sizeof(temp));
        num_phases = 5;
    } else {
        Phase temp[] = { GATE, TAXI, TAKEOFF, CLIMB, CRUISE };
        memcpy(phases, temp, sizeof(temp));
        num_phases = 5;
    }

    for (int i = 0; i < num_phases && simulation_running; ++i) {
        pthread_mutex_lock(&aircraft->lock);
        aircraft->current_phase = phases[i];

        // Calculate speed limits based on phase
        int min_speed, max_speed;
        if (aircraft->direction == ARRIVAL) {
            min_speed = ARRIVAL_SPEED_LIMITS[i][0];
            max_speed = ARRIVAL_SPEED_LIMITS[i][1];
        } else {
            min_speed = DEPARTURE_SPEED_LIMITS[i][0];
            max_speed = DEPARTURE_SPEED_LIMITS[i][1];
        }

        // Randomized speed within phase range, adding some randomness
        aircraft->speed = min_speed + rand() % (max_speed - min_speed + 20);

        safe_print("[Flight " + string(aircraft->flight_number) + "] [" +
                   (aircraft->direction == ARRIVAL ? "ARRIVAL" : "DEPARTURE") +
                   "] Phase: " + PHASE_NAMES[phases[i]] + ", Speed: " +
                   to_string(aircraft->speed) + " km/h");
        pthread_mutex_unlock(&aircraft->lock);

        // If phase needs runway, request runway
        bool needs_runway = (aircraft->direction == ARRIVAL && phases[i] == LANDING) ||
                            (aircraft->direction == DEPARTURE && phases[i] == TAKEOFF);

              if (needs_runway) {
            Runway* assigned_runway = request_runway(aircraft->flight_number, aircraft->type); // [MODULE 2]
            if (assigned_runway) {
                sleep(3); // Simulate takeoff/landing
                release_runway(assigned_runway);
            }
        } else {
            sleep(3); // Simulate phase time
        }
// Update position
        pthread_mutex_lock(&aircraft->lock);
        aircraft->position.x += 20;  // Move forward per phase (example logic)
        pthread_mutex_unlock(&aircraft->lock);
    }

    pthread_mutex_lock(&aircraft->lock);
    aircraft->is_active = false;
    pthread_mutex_unlock(&aircraft->lock);
    return nullptr;
}

/*
Radar thread constantly checks each aircraft for speed compliance.
If a violation is detected (too slow or too fast), it triggers an
AVN. Aircrafts are only issued a violation once.
*/

void* radar_monitor(void* arg) {
    while (simulation_running) {
        for (int i = 0; i < NUM_AIRCRAFTS; ++i) {
            pthread_mutex_lock(&aircrafts[i].lock);
            if (!aircrafts[i].avn_issued && aircrafts[i].is_active) {
                Phase phase = aircrafts[i].current_phase;
                int speed = aircrafts[i].speed;
                int min_speed, max_speed;

                if (aircrafts[i].direction == ARRIVAL) {
                    min_speed = ARRIVAL_SPEED_LIMITS[phase][0];
                    max_speed = ARRIVAL_SPEED_LIMITS[phase][1];
                } else {
                    min_speed = DEPARTURE_SPEED_LIMITS[phase][0];
                    max_speed = DEPARTURE_SPEED_LIMITS[phase][1];
                }

                if (speed < min_speed || speed > max_speed) {
                    issue_avn(&aircrafts[i], ("Speed violation in phase " + string(PHASE_NAMES[phase])).c_str());
                }
            }
            pthread_mutex_unlock(&aircrafts[i].lock);
        }
        usleep(500000); // 0.5s delay before next check
    }
    return nullptr;
}

// Controls simulation time (default 5 minutes)
// Controls simulation time and handles restart scenarios

void* simulation_timer(void* arg) {
    int duration = 300; // Default 5 minutes
    while (simulation_running) {
        sleep(50); // Timer for 5 minutes
        safe_print("\nSimulation Time Ended.");
        simulation_running = false; // Mark the simulation as finished
    }
    return nullptr;
}
void drawPhaseBoundaries(sf::RenderWindow& window) {
    const float PHASE_LINE_OFFSET = 100.f;  // Offset to move the lines to the right
    for (float x : PHASE_X_POSITIONS) {
        sf::RectangleShape line(sf::Vector2f(2.f, 600.f)); // tall vertical line
        line.setPosition(x + PHASE_LINE_OFFSET, 0.f);  // Apply the offset to the x position
        line.setFillColor(sf::Color(150, 150, 150)); // Light gray for the phase boundary
        window.draw(line);
    }
}


// Render aircraft according to the flight phase
void render_aircraft(sf::RenderWindow& window, const Aircraft& aircraft, sf::Texture &texture) {
    sf::Sprite sprite;
    sprite.setTexture(texture);
    sprite.setScale(0.1f, 0.1f);  // Adjust size
 // Draw dotted lines between each phase location
    

    // Dynamic position based on aircraft phase
    float y_position = 100.f + 50.f * aircraft.type;  // Vertical placement by aircraft type
int index = 0;

if (aircraft.direction == ARRIVAL) {
    // ARRIVAL phases: HOLDING(0) to GATE(4)
    if (aircraft.current_phase <= GATE) {
        index = static_cast<int>(aircraft.current_phase);
    }
} else {
    // DEPARTURE phases: GATE(4) to CRUISE(8)
    if (aircraft.current_phase >= GATE && aircraft.current_phase <= CRUISE) {
        index = static_cast<int>(aircraft.current_phase) - GATE;
    }
}

sprite.setPosition(PHASE_X_POSITIONS[index], y_position);

     
   
    window.draw(sprite);  // Draw the aircraft sprite
}

// Render runways with visual representation of status
void render_runways(sf::RenderWindow& window) {
    // Render three runways at different Y positions or across the screen
    for (int i = 0; i < 3; ++i) {
        sf::RectangleShape runway(sf::Vector2f(800.f, 20.f)); // Adjust size accordingly
        runway.setTexture(&runwayTexture);
        
        // Set runway color to indicate if it's occupied
        if (runwaysInUse[i]) {
            runway.setFillColor(sf::Color::Red); // Occupied runway
        } else {
            runway.setFillColor(sf::Color::Green); // Available runway
        }

        runway.setPosition(100.f, 150.f + (i * 100)); // Adjust Y position for each runway
        window.draw(runway);
    }
}


// ========================== MAIN FUNCTION ===================================
int main() {
 
sf::Font font;


sf::Text clockText;
clockText.setFont(font);
clockText.setCharacterSize(20);
clockText.setFillColor(sf::Color::White);
clockText.setPosition(10, 10);  // top-left corner
if (!font.loadFromFile("/mnt/c/Users/USR/Downloads/arial/ARIAL.TTF")) {
    std::cout << "Failed to load font\n";
}

if (!commercialTexture.loadFromFile("/mnt/c/Users/USR/Downloads/newairplane.png")) {
    std::cout << "Failed to load commercial texture\n";
}

if (!cargoTexture.loadFromFile("/mnt/c/Users/USR/Downloads/newcargo.png")) {
    std::cout << "Failed to load cargo texture\n";
}

if (!emergencyTexture.loadFromFile("/mnt/c/Users/USR/Downloads/emergency.png")) {
    std::cout << "Failed to load emergency texture\n";
}

if (!runwayTexture.loadFromFile("/mnt/c/Users/USR/Downloads/runway.png")) {
    std::cout << "Failed to load runway texture\n";
}
std::ofstream clearLog("avnlog.txt", std::ofstream::out | std::ofstream::trunc);
    clearLog.close();
FILE* clear_log = fopen("/mnt/c/Users/USR/Downloads/avn_log.txt", "w");
if (clear_log) fclose(clear_log); // Clears previous run

srand(time(NULL));


    printf("🧚✈️ AIR TRAFFIC CONTROL SIMULATOR ✈️🧚\n   BY EMAN IHSAN AND FATIMA TUZ ZAHRA\n\n");
std::cout << "Phase Breakdown for the AirControlX Simulation:" << std::endl;
std::cout << "--------------------------------------------" << std::endl;
std::cout << "Phase 1 (100): Holding or Gate - The aircraft is either waiting for clearance (holding) or at the gate for boarding and final checks." << std::endl;
std::cout << "Phase 2 (200): Approach or Taxi - The aircraft is either taxiing to the runway (taxi) or approaching the runway for takeoff." << std::endl;
std::cout << "Phase 3 (300): Landing or Takeoff - The aircraft is either taking off from the runway or landing at the destination." << std::endl;
std::cout << "Phase 4 (400): Taxi or Climb - After takeoff, the aircraft either taxis on the ground or climbs to cruising altitude." << std::endl;
std::cout << "Phase 5 (500): Gate or Cruise - The aircraft is either at the gate after landing or cruising at a high altitude." << std::endl;
std::cout << "--------------------------------------------" << std::endl;
std::cout << "These phases represent key stages of an aircraft's journey, with overlapping roles for each phase." << std::endl;

    const char* flight_ids[NUM_AIRCRAFTS] = { "PK303", "FX101", "ED220", "AF001", "BD321", "AK911" };
    AircraftType types[NUM_AIRCRAFTS] = { COMMERCIAL, CARGO, COMMERCIAL, EMERGENCY, CARGO, EMERGENCY };
    FlightType directions[NUM_AIRCRAFTS] = { ARRIVAL, ARRIVAL, DEPARTURE, DEPARTURE, ARRIVAL, DEPARTURE };

    pthread_t flight_threads[NUM_AIRCRAFTS];
    pthread_t radar_thread, timer_thread;

    // Initialize aircrafts
    for (int i = 0; i < NUM_AIRCRAFTS; ++i) {
        strncpy(aircrafts[i].flight_number, flight_ids[i], 10);
        aircrafts[i].type = types[i];
        aircrafts[i].direction = directions[i];
        aircrafts[i].current_phase = GATE;
        aircrafts[i].speed = 0;
        aircrafts[i].is_active = true;
        aircrafts[i].avn_issued = false;
        aircrafts[i].position = sf::Vector2f(50.f, 100.f + 70.f * i);  // Initial Y offset
        pthread_mutex_init(&aircrafts[i].lock, NULL);
    }
//pthread_t radar_thread;
pthread_create(&radar_thread, NULL, radar_monitor, NULL);

    // Create aircraft threads
    for (int i = 0; i < NUM_AIRCRAFTS; ++i) {
        pthread_create(&flight_threads[i], NULL, flight_simulation, (void*)&aircrafts[i]);
    }

    pthread_create(&radar_thread, NULL, radar_monitor, NULL);
    pthread_create(&timer_thread, NULL, simulation_timer, NULL);

    // === SFML VISUALIZATION LOOP ===
    sf::RenderWindow window(sf::VideoMode(1000, 600), "Air Traffic Control - Visualizer");

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear(sf::Color::Black);
drawPhaseBoundaries(window); // before drawing aircraft
        // Draw all three runways
    render_runways(window);

        for (int i = 0; i < NUM_AIRCRAFTS; ++i) {
    pthread_mutex_lock(&aircrafts[i].lock);
    sf::Texture* texture = nullptr;
    switch (aircrafts[i].type) {
        case COMMERCIAL: texture = &commercialTexture; break;
        case CARGO:      texture = &cargoTexture; break;
        case EMERGENCY:  texture = &emergencyTexture; break;
    }
    render_aircraft(window, aircrafts[i], *texture);
    pthread_mutex_unlock(&aircrafts[i].lock);
}


        // Get current system time
        auto now = std::chrono::system_clock::now();
        std::time_t time_now = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_now), "%H:%M:%S");

        // Set it to the text object
        clockText.setString("Time: " + ss.str());

        // Draw the clock
        window.draw(clockText);

        window.display();
    }

    // Wait for threads after SFML window is closed
    for (int i = 0; i < NUM_AIRCRAFTS; ++i) {
        pthread_join(flight_threads[i], NULL);
    }
    pthread_join(radar_thread, NULL);
    pthread_join(timer_thread, NULL);

// Stop radar monitor
    simulation_running = false;
    pthread_join(radar_thread, NULL);
    pid_t pid = fork();
    if (pid == 0) {
        // CHILD PROCESS: Airline Billing Portal
        safe_print("\n🧾 Launching Airline Billing Portal...\n");

        FILE* f = fopen("avn_log.txt", "r");
        if (!f) {
            safe_print("No AVNs to process. All aircrafts compliant.");
            exit(0);
        }

        char line[256];
        int total_fine = 0;
        safe_print("📋 AVN Fine Summary:");

        while (fgets(line, sizeof(line), f)) {
            printf("%s", line);

            // Extract fine amount from end of line
            char* dollar = strrchr(line, '$');
            if (dollar) {
                total_fine += atoi(dollar + 1);
            }
        }
        fclose(f);

        printf("\n💰 Total Fine Amount Due: $%d\n", total_fine);
        printf("✅ Processing payment... Payment successful.\n");
        exit(0);
    }

    // Destroy all aircraft locks
    for (int i = 0; i < NUM_AIRCRAFTS; ++i) {
        pthread_mutex_destroy(&aircrafts[i].lock);
    }

    safe_print("\nSimulation complete. All aircraft have completed their operations.");
    return 0;
}
