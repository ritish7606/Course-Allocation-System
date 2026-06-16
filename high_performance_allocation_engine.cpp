#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <bitset>
#include <cstdint>
#include <algorithm>

// Hard limits for contiguous memory pre-allocation
constexpr uint16_t MAX_COURSES = 1024;
constexpr uint16_t MAX_STUDENTS = 20000;
constexpr uint16_t NO_EVICTION = 65535;

// --- DATA STRUCTURES ---
struct StudentData {
    uint16_t cgpa_scaled = 0; 
    std::bitset<MAX_COURSES> completed_courses; 
    uint32_t booked_slots = 0;                  
    std::vector<uint16_t> enrolled_courses;     
    std::vector<uint16_t> preference_list;      
};

struct CourseData {
    uint16_t capacity = 0;
    std::bitset<MAX_COURSES> prerequisites;     
    uint32_t time_slot = 0;                     
    std::vector<uint16_t> enrolled_students; // Managed as a Min-Heap
};

// --- ENGINE ---
class FastAllocationEngine {
private:
    std::vector<StudentData> students;
    std::vector<CourseData> courses;

    // Translation Dictionaries (Used only at I/O boundaries)
    std::unordered_map<std::string, uint16_t> student_id_map;
    std::unordered_map<std::string, uint16_t> course_id_map;
    
    // Reverse dictionaries for printing results
    std::vector<std::string> reverse_student_map;
    std::vector<std::string> reverse_course_map;

    uint16_t next_student_id = 0;
    uint16_t next_course_id = 0;

    uint16_t get_course_id(const std::string& str_id) {
        auto it = course_id_map.find(str_id);
        if (it != course_id_map.end()) return it->second;
        uint16_t new_id = next_course_id++;
        course_id_map[str_id] = new_id;
        if (new_id >= reverse_course_map.size()) reverse_course_map.resize(new_id + 100);
        reverse_course_map[new_id] = str_id;
        return new_id;
    }

    uint16_t get_student_id(const std::string& str_id) {
        auto it = student_id_map.find(str_id);
        if (it != student_id_map.end()) return it->second;
        uint16_t new_id = next_student_id++;
        student_id_map[str_id] = new_id;
        if (new_id >= reverse_student_map.size()) reverse_student_map.resize(new_id + 100);
        reverse_student_map[new_id] = str_id;
        return new_id;
    }

    bool compare_cgpa(uint16_t id_a, uint16_t id_b) {
        return students[id_a].cgpa_scaled > students[id_b].cgpa_scaled;
    }

    uint16_t enroll_with_priority(uint16_t s_id, uint16_t c_id) {
        StudentData& student = students[s_id];
        CourseData& course = courses[c_id];

        // Hardware-level constraint checks
        if ((student.booked_slots & course.time_slot) != 0) return s_id; 
        if ((student.completed_courses & course.prerequisites) != course.prerequisites) return s_id; 

        // Capacity & Priority Logic
        if (course.enrolled_students.size() < course.capacity) {
            course.enrolled_students.push_back(s_id);
            std::push_heap(course.enrolled_students.begin(), course.enrolled_students.end(), 
                           [this](uint16_t a, uint16_t b) { return compare_cgpa(a, b); });
            
            student.booked_slots |= course.time_slot;
            return NO_EVICTION;
        } 
        else {
            uint16_t worst_student_id = course.enrolled_students.front(); 
            
            if (student.cgpa_scaled > students[worst_student_id].cgpa_scaled) {
                std::pop_heap(course.enrolled_students.begin(), course.enrolled_students.end(), 
                              [this](uint16_t a, uint16_t b) { return compare_cgpa(a, b); });
                course.enrolled_students.pop_back();

                course.enrolled_students.push_back(s_id);
                std::push_heap(course.enrolled_students.begin(), course.enrolled_students.end(), 
                               [this](uint16_t a, uint16_t b) { return compare_cgpa(a, b); });

                students[worst_student_id].booked_slots &= ~(course.time_slot); 
                student.booked_slots |= course.time_slot;

                return worst_student_id; 
            }
            return s_id; 
        }
    }

public:
    FastAllocationEngine() {
        students.resize(MAX_STUDENTS);
        courses.resize(MAX_COURSES);
    }

    void add_student(const std::string& str_id, float cgpa, const std::vector<std::string>& completed_strs, const std::vector<std::string>& preferences) {
        uint16_t s_id = get_student_id(str_id);
        StudentData& student = students[s_id];
        
        student.cgpa_scaled = static_cast<uint16_t>(cgpa * 100); // Scale float to int
        
        for (const auto& c_str : completed_strs) {
            student.completed_courses.set(get_course_id(c_str)); 
        }
        for (const auto& p_str : preferences) {
            student.preference_list.push_back(get_course_id(p_str));
        }
    }

    void add_course(const std::string& str_id, uint16_t capacity, uint32_t slot_mask, const std::vector<std::string>& prereq_strs) {
        uint16_t c_id = get_course_id(str_id);
        CourseData& course = courses[c_id];
        
        course.capacity = capacity;
        course.time_slot = slot_mask;
        course.enrolled_students.reserve(capacity); 
        
        for (const auto& p_str : prereq_strs) {
            course.prerequisites.set(get_course_id(p_str)); 
        }
    }

    void execute_matching() {
        std::vector<uint16_t> next_pref_index(MAX_STUDENTS, 0);
        std::vector<uint16_t> free_students;
        free_students.reserve(MAX_STUDENTS);

        for (uint16_t i = 0; i < next_student_id; ++i) {
            free_students.push_back(i);
        }

        while (!free_students.empty()) {
            uint16_t current_student_id = free_students.back();
            free_students.pop_back();

            StudentData& student = students[current_student_id];
            uint16_t pref_idx = next_pref_index[current_student_id];

            if (pref_idx >= student.preference_list.size()) continue; 

            uint16_t target_course_id = student.preference_list[pref_idx];
            next_pref_index[current_student_id]++;

            uint16_t evicted_id = enroll_with_priority(current_student_id, target_course_id);

            if (evicted_id == current_student_id) {
                free_students.push_back(current_student_id);
            } 
            else if (evicted_id != NO_EVICTION) {
                free_students.push_back(evicted_id);
            }
        }
    }

    void print_results() {
        std::cout << "\n--- ALLOCATION RESULTS ---\n";
        for (uint16_t c_id = 0; c_id < next_course_id; ++c_id) {
            std::cout << "Course: " << reverse_course_map[c_id] << " (" 
                      << courses[c_id].enrolled_students.size() << "/" 
                      << courses[c_id].capacity << " seats filled)\n";
            
            for (uint16_t s_id : courses[c_id].enrolled_students) {
                std::cout << "  - " << reverse_student_map[s_id] 
                          << " (CGPA: " << students[s_id].cgpa_scaled / 100.0 << ")\n";
            }
        }
    }
};

// --- DRIVER CODE ---
int main() {
    FastAllocationEngine engine;

    // 1. Define Courses
    // add_course(ID, Capacity, SlotBitmask, Prerequisites)
    // Bitmask: 1 = Slot A, 2 = Slot B, 4 = Slot C, 8 = Slot D
    engine.add_course("CS101", 100, 1, {});                  // Slot A
    engine.add_course("CS201", 2, 2, {"CS101"});             // Slot B, highly constrained capacity of 2
    engine.add_course("CS301", 5, 4, {"CS101", "CS201"});    // Slot C

    // 2. Define Students
    // add_student(ID, CGPA, CompletedCourses, PreferenceList)
    
    // Alice has high CGPA, wants CS201
    engine.add_student("STU_ALICE", 9.8, {"CS101"}, {"CS201", "CS301"}); 
    
    // Bob has low CGPA, wants CS201
    engine.add_student("STU_BOB", 7.5, {"CS101"}, {"CS201", "CS301"});   
    
    // Charlie has high CGPA, wants CS201
    engine.add_student("STU_CHARLIE", 9.2, {"CS101"}, {"CS201"});        
    
    // Dave has average CGPA, wants CS201 but is missing a prerequisite
    engine.add_student("STU_DAVE", 8.5, {}, {"CS201"});                  

    // 3. Run the highly-optimized matching engine
    engine.execute_matching();

    // 4. Output the results
    engine.print_results();

    return 0;
}