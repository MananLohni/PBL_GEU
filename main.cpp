#include "Optional.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "Subject.hpp"
#include "Topic.hpp"
#include "SubjectManager.hpp"
#include "TopicManager.hpp"
#include "SearchSortEngine.hpp"
#include "PriorityEngine.hpp"

// This flat console project reads subjects.json and topics.json from its
// working directory. run_console.bat changes to the project folder first.

static std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    if (!std::getline(std::cin, value)) return "";
    return value;
}

static int readInt(const std::string& prompt, int minimum, int maximum) {
    while (true) {
        const std::string input = readLine(prompt);
        try {
            size_t used = 0;
            const int value = std::stoi(input, &used);
            if (used == input.size() && value >= minimum && value <= maximum) return value;
        } catch (...) {
        }
        if (!std::cin) return minimum;
        std::cout << "Enter a whole number from " << minimum << " to " << maximum << ".\n";
    }
}

static double readDouble(const std::string& prompt, double minimum, double maximum) {
    while (true) {
        const std::string input = readLine(prompt);
        try {
            size_t used = 0;
            const double value = std::stod(input, &used);
            if (used == input.size() && std::isfinite(value) && value >= minimum && value <= maximum)
                return value;
        } catch (...) {
        }
        if (!std::cin) return minimum;
        std::cout << "Enter a number from " << minimum << " to " << maximum << ".\n";
    }
}

static std::string readStatus() {
    std::cout << "1. Not started\n2. In progress\n3. Completed\n";
    const int choice = readInt("Choose status: ", 1, 3);
    if (choice == 2) return "IN_PROGRESS";
    if (choice == 3) return "COMPLETED";
    return "NOT_STARTED";
}

static std::string getSubjectName(const SubjectManager& manager, const std::string& id) {
    const auto subject = manager.getSubjectById(id);
    return subject ? subject->getName() : "[Missing subject]";
}

static void printDivider() {
    std::cout << "--------------------------------------------------------------------------\n";
}

static void listSubjects(const SubjectManager& subjects, const TopicManager& topics) {
    std::cout << "\nSUBJECT RECORDS (" << subjects.count() << ")\n";
    printDivider();
    if (subjects.getAllSubjects().empty()) {
        std::cout << "No subjects found.\n";
        return;
    }
    for (const auto& subject : subjects.getAllSubjects()) {
        std::cout << "Name: " << subject.getName() << "\n"
                  << "  ID: " << subject.getId() << "\n"
                  << "  Code: " << (subject.getCode().empty() ? "N/A" : subject.getCode()) << "\n"
                  << "  Semester: " << (subject.getSemester().empty() ? "N/A" : subject.getSemester()) << "\n"
                  << "  Exam date: " << subject.getExamDate() << "\n"
                  << "  Topics: " << topics.countTopicsForSubject(subject.getId()) << "\n";
        printDivider();
    }
}

static void listTopics(const TopicManager& topics, const SubjectManager& subjects,
                       const std::string& subjectFilter = "") {
    const auto records = topics.getAllTopics(subjectFilter);
    std::cout << "\nTOPIC RECORDS (" << records.size() << ")\n";
    printDivider();
    if (records.empty()) {
        std::cout << "No topics found.\n";
        return;
    }
    std::cout << std::fixed << std::setprecision(1);
    for (const auto& topic : records) {
        std::cout << "Name: " << topic.getName() << "\n"
                  << "  ID: " << topic.getId() << "\n"
                  << "  Subject: " << getSubjectName(subjects, topic.getSubjectId()) << "\n"
                  << "  Subject ID: " << topic.getSubjectId() << "\n"
                  << "  Difficulty: " << topic.getDifficulty() << "/5\n"
                  << "  Syllabus remaining: " << topic.getSyllabusRemaining() << "%\n"
                  << "  Academic importance: " << topic.getAcademicImportance() << "/5\n"
                  << "  Estimated hours: " << topic.getEstimatedHours() << "\n"
                  << "  Status: " << topic.getStatus() << "\n"
                  << "  Priority: " << topic.getPriority() << "/100\n";
        printDivider();
    }
}

static void listRecommendations(const SubjectManager& subjects, const TopicManager& topics,
                               const std::string& subjectId = "") {
    const auto recommendations = PriorityEngine::recommend(
        subjects.getAllSubjects(), topics.getAllTopics(), subjectId);
    std::cout << "\nSMART STUDY RECOMMENDATIONS\n";
    printDivider();
    if (recommendations.empty()) {
        std::cout << "No eligible recommendations. Topics may be completed or have 0% syllabus remaining.\n";
        return;
    }
    int rank = 1;
    for (const auto& item : recommendations) {
        const auto& t = item.topic;
        const auto& b = item.breakdown;
        std::cout << "#" << rank++ << "  " << t.getName()
                  << " | Priority: " << std::fixed << std::setprecision(1) << b.score << "/100\n"
                  << "    Subject: " << item.subjectName
                  << " | Exam: " << (item.examDate.empty() ? "N/A" : item.examDate);
        if (b.hasValidExamDate && b.daysUntilExam >= 0)
            std::cout << " (" << b.daysUntilExam << " days)";
        std::cout << "\n"
                  << "    Urgency contribution: " << (0.30 * b.examUrgency)
                  << " | Syllabus: " << (0.25 * b.syllabusRemaining)
                  << " | Importance: " << (0.20 * b.academicImportance) << "\n"
                  << "    Difficulty: " << (0.10 * b.difficulty)
                  << " | Subject workload: " << (0.15 * b.subjectWorkload) << "\n";
        printDivider();
    }
}

static bool refreshPriorities(SubjectManager& subjects, TopicManager& topics) {
    const auto recommendations = PriorityEngine::recommend(
        subjects.getAllSubjects(), topics.getAllTopics());
    std::unordered_map<std::string, double> priorities;
    for (const auto& recommendation : recommendations)
        priorities[recommendation.topic.getId()] = recommendation.breakdown.score;
    return topics.updatePriorities(priorities);
}

static std::string askSubjectId(const SubjectManager& subjects, const TopicManager& topics) {
    if (subjects.getAllSubjects().empty()) {
        std::cout << "Create a subject before using this operation.\n";
        return "";
    }
    listSubjects(subjects, topics);
    const std::string id = readLine("Enter the subject ID shown above: ");
    if (!subjects.hasSubject(id)) {
        std::cout << "Subject ID not found.\n";
        return "";
    }
    return id;
}

static std::string askTopicId(const TopicManager& topics, const SubjectManager& subjects) {
    if (topics.getAllTopics().empty()) {
        std::cout << "No topics available.\n";
        return "";
    }
    listTopics(topics, subjects);
    const std::string id = readLine("Enter the topic ID shown above: ");
    if (!topics.getTopicById(id)) {
        std::cout << "Topic ID not found.\n";
        return "";
    }
    return id;
}

static void addSubject(SubjectManager& subjects, TopicManager& topics) {
    std::cout << "\nADD SUBJECT\n";
    const std::string name = readLine("Subject name: ");
    const std::string code = readLine("Course code (optional): ");
    const std::string semester = readLine("Semester/term (optional): ");
    const std::string date = readLine("Exam date (YYYY-MM-DD): ");
    std::string error;
    const auto created = subjects.addSubject(name, code, semester, date, error);
    if (!created) {
        std::cout << "Could not create subject: " << error << "\n";
        return;
    }
    if (!refreshPriorities(subjects, topics))
        std::cout << "Subject created, but priority persistence needs attention.\n";
    std::cout << "Subject created. ID: " << created->getId() << "\n";
}

static void editSubject(SubjectManager& subjects, TopicManager& topics) {
    const std::string id = askSubjectId(subjects, topics);
    if (id.empty()) return;
    const auto current = subjects.getSubjectById(id);
    if (!current) return;
    std::cout << "1. Name\n2. Course code\n3. Semester\n4. Exam date\n";
    const int field = readInt("Field to edit: ", 1, 4);
    nlohmann::json payload;
    if (field == 1) payload["name"] = readLine("New name: ");
    else if (field == 2) payload["code"] = readLine("New course code: ");
    else if (field == 3) payload["semester"] = readLine("New semester/term: ");
    else payload["examDate"] = readLine("New exam date (YYYY-MM-DD): ");
    std::string error;
    const auto updated = subjects.updateSubject(id, payload, error);
    if (!updated) {
        std::cout << "Could not update subject: " << error << "\n";
        return;
    }
    if (!refreshPriorities(subjects, topics))
        std::cout << "Subject updated, but priority persistence needs attention.\n";
    else std::cout << "Subject updated and priorities refreshed.\n";
}

static void deleteSubject(SubjectManager& subjects, TopicManager& topics) {
    const std::string id = askSubjectId(subjects, topics);
    if (id.empty()) return;
    std::string error;
    const bool deleted = subjects.deleteSubject(id,
        [&topics](const std::string& subjectId) { return topics.hasTopicsForSubject(subjectId); }, error);
    if (!deleted) {
        std::cout << "Could not delete subject: " << error << "\n";
        return;
    }
    if (!refreshPriorities(subjects, topics))
        std::cout << "Subject deleted, but priority persistence needs attention.\n";
    else std::cout << "Subject deleted.\n";
}

static void addTopic(SubjectManager& subjects, TopicManager& topics) {
    if (subjects.getAllSubjects().empty()) {
        std::cout << "Create a subject first.\n";
        return;
    }
    std::cout << "\nADD TOPIC\n";
    listSubjects(subjects, topics);
    const std::string subjectId = readLine("Enter associated subject ID: ");
    if (!subjects.hasSubject(subjectId)) {
        std::cout << "Subject ID not found.\n";
        return;
    }
    nlohmann::json payload;
    payload["subjectId"] = subjectId;
    payload["name"] = readLine("Topic name: ");
    payload["difficulty"] = readInt("Difficulty (1-5): ", 1, 5);
    payload["syllabusRemaining"] = readDouble("Syllabus remaining (0-100): ", 0.0, 100.0);
    payload["academicImportance"] = readInt("Academic importance (1-5): ", 1, 5);
    payload["estimatedHours"] = readDouble("Estimated study hours (>0): ", 0.1, 10000.0);
    payload["status"] = readStatus();
    if (payload["status"] == "COMPLETED") payload["syllabusRemaining"] = 0.0;
    std::string error;
    const auto created = topics.addTopic(payload, subjects, error);
    if (!created) {
        std::cout << "Could not create topic: " << error << "\n";
        return;
    }
    if (!refreshPriorities(subjects, topics))
        std::cout << "Topic created, but priority persistence needs attention.\n";
    std::cout << "Topic created. ID: " << created->getId() << "\n";
}

static void editTopic(SubjectManager& subjects, TopicManager& topics) {
    const std::string id = askTopicId(topics, subjects);
    if (id.empty()) return;
    const auto current = topics.getTopicById(id);
    if (!current) return;
    std::cout << "1. Name\n2. Subject\n3. Difficulty\n4. Syllabus remaining\n"
              << "5. Academic importance\n6. Estimated study hours\n7. Status\n";
    const int field = readInt("Field to edit: ", 1, 7);
    nlohmann::json payload;
    if (field == 1) payload["name"] = readLine("New topic name: ");
    else if (field == 2) {
        const std::string subjectId = askSubjectId(subjects, topics);
        if (subjectId.empty()) return;
        payload["subjectId"] = subjectId;
    } else if (field == 3) payload["difficulty"] = readInt("Difficulty (1-5): ", 1, 5);
    else if (field == 4) payload["syllabusRemaining"] = readDouble("Syllabus remaining (0-100): ", 0.0, 100.0);
    else if (field == 5) payload["academicImportance"] = readInt("Academic importance (1-5): ", 1, 5);
    else if (field == 6) payload["estimatedHours"] = readDouble("Estimated study hours (>0): ", 0.1, 10000.0);
    else payload["status"] = readStatus();

    std::string error;
    Optional<Topic> updated;
    if (field == 7) {
        updated = topics.updateTopicStatus(id, payload["status"].get<std::string>(), error);
    } else {
        updated = topics.updateTopic(id, payload, subjects, error);
    }
    if (!updated) {
        std::cout << "Could not update topic: " << error << "\n";
        return;
    }
    if (!refreshPriorities(subjects, topics))
        std::cout << "Topic updated, but priority persistence needs attention.\n";
    else std::cout << "Topic updated and priorities refreshed.\n";
}

static void deleteTopic(SubjectManager& subjects, TopicManager& topics) {
    const std::string id = askTopicId(topics, subjects);
    if (id.empty()) return;
    std::string error;
    if (!topics.deleteTopic(id, error)) {
        std::cout << "Could not delete topic: " << error << "\n";
        return;
    }
    if (!refreshPriorities(subjects, topics))
        std::cout << "Topic deleted, but priority persistence needs attention.\n";
    else std::cout << "Topic deleted.\n";
}

static void searchTopics(const SubjectManager& subjects, const TopicManager& topics) {
    const std::string query = readLine("Search topic name, status, or subject name: ");
    const auto result = SearchSortEngine::linearSearchTopics(topics.getAllTopics(), query,
        [&subjects](const std::string& id) { return getSubjectName(subjects, id); });
    if (result.empty()) std::cout << "No matching topics found.\n";
    else {
        std::cout << "Found " << result.size() << " matching topic(s).\n";
        // Print a filtered list using existing topic display conventions.
        for (const auto& topic : result)
            std::cout << "- " << topic.getName() << " | " << getSubjectName(subjects, topic.getSubjectId())
                      << " | " << topic.getStatus() << " | Priority " << topic.getPriority() << "\n";
    }
}

static void searchSubjects(const SubjectManager& subjects) {
    const std::string query = readLine("Search subject name, code, semester, or exam date: ");
    const auto result = SearchSortEngine::linearSearchSubjects(subjects.getAllSubjects(), query);
    if (result.empty()) std::cout << "No matching subjects found.\n";
    else for (const auto& subject : result)
        std::cout << "- " << subject.getName() << " | " << subject.getCode()
                  << " | " << subject.getSemester() << " | Exam " << subject.getExamDate() << "\n";
}

static void exactTopicSearch(const SubjectManager& subjects, const TopicManager& topics) {
    const std::string query = readLine("Enter exact topic name: ");
    const auto result = SearchSortEngine::binarySearchTopicExact(topics.getAllTopics(), query);
    if (!result) {
        std::cout << "Exact topic not found.\n";
        return;
    }
    std::cout << "Found: " << result->getName() << " | Subject: "
              << getSubjectName(subjects, result->getSubjectId()) << " | ID: " << result->getId() << "\n";
}

static void sortTopics(const SubjectManager& subjects, const TopicManager& topics) {
    std::cout << "1. Topic name\n2. Difficulty\n3. Syllabus remaining\n"
              << "4. Academic importance\n5. Estimated hours\n";
    const int choice = readInt("Sort criterion: ", 1, 5);
    const std::vector<std::string> keys = {"name", "difficulty", "syllabusRemaining", "academicImportance", "estimatedHours"};
    const bool ascending = readInt("1. Ascending\n2. Descending\nChoose order: ", 1, 2) == 1;
    const auto result = SearchSortEngine::sortTopics(topics.getAllTopics(), keys[choice - 1], ascending);
    for (const auto& t : result)
        std::cout << "- " << t.getName() << " | Subject: " << getSubjectName(subjects, t.getSubjectId())
                  << " | Difficulty " << t.getDifficulty() << "/5 | Syllabus " << t.getSyllabusRemaining()
                  << "% | Hours " << t.getEstimatedHours() << "\n";
}

static void sortSubjects(const SubjectManager& subjects) {
    std::cout << "1. Subject name\n2. Course code\n3. Semester\n4. Exam date\n";
    const int choice = readInt("Sort criterion: ", 1, 4);
    const std::vector<std::string> keys = {"name", "code", "semester", "examDate"};
    const bool ascending = readInt("1. Ascending\n2. Descending\nChoose order: ", 1, 2) == 1;
    const auto result = SearchSortEngine::sortSubjects(subjects.getAllSubjects(), keys[choice - 1], ascending);
    for (const auto& s : result)
        std::cout << "- " << s.getName() << " | " << s.getCode() << " | " << s.getSemester()
                  << " | Exam " << s.getExamDate() << "\n";
}

static void showMenu() {
    std::cout << "\n================ STUDYWISE BACKEND MENU ================\n"
              << " 1. View subjects\n"
              << " 2. Add subject\n"
              << " 3. Edit subject\n"
              << " 4. Delete subject\n"
              << " 5. View topics\n"
              << " 6. Add topic\n"
              << " 7. Edit topic\n"
              << " 8. Delete topic\n"
              << " 9. Linear search topics\n"
              << "10. Linear search subjects\n"
              << "11. Exact topic lookup (Binary Search)\n"
              << "12. Sort topics (Merge Sort)\n"
              << "13. Sort subjects (Merge Sort)\n"
              << "14. Smart recommendations (all subjects)\n"
              << "15. Smart recommendations (selected subject)\n"
              << " 0. Save/exit\n"
              << "========================================================\n";
}

int main() {
    std::cout << "========================================================\n"
              << " STUDYWISE — Standalone C++ Backend Demonstration\n"
              << " OOP | JSON Persistence | Searching | Sorting | Max Heap\n"
              << "========================================================\n";

    const std::string subjectsPath = "subjects.json";
    const std::string topicsPath = "topics.json";
    SubjectManager subjectManager(subjectsPath);
    TopicManager topicManager(topicsPath);

    if (!subjectManager.loadFromFile() || !topicManager.loadFromFile()) {
        std::cerr << "Startup aborted because existing data could not be loaded. Data was not intentionally reset.\n";
        return 1;
    }
    std::cout << "Loaded " << subjectManager.count() << " subject(s) and "
              << topicManager.count() << " topic(s).\n";
    if (!refreshPriorities(subjectManager, topicManager)) {
        std::cerr << "Warning: priority scores could not be saved. Check write permissions for the project folder.\n";
    }

    bool running = true;
    while (running && std::cin) {
        showMenu();
        const int choice = readInt("Choose an option: ", 0, 15);
        switch (choice) {
            case 1: listSubjects(subjectManager, topicManager); break;
            case 2: addSubject(subjectManager, topicManager); break;
            case 3: editSubject(subjectManager, topicManager); break;
            case 4: deleteSubject(subjectManager, topicManager); break;
            case 5: listTopics(topicManager, subjectManager); break;
            case 6: addTopic(subjectManager, topicManager); break;
            case 7: editTopic(subjectManager, topicManager); break;
            case 8: deleteTopic(subjectManager, topicManager); break;
            case 9: searchTopics(subjectManager, topicManager); break;
            case 10: searchSubjects(subjectManager); break;
            case 11: exactTopicSearch(subjectManager, topicManager); break;
            case 12: sortTopics(subjectManager, topicManager); break;
            case 13: sortSubjects(subjectManager); break;
            case 14: listRecommendations(subjectManager, topicManager); break;
            case 15: {
                const std::string subjectId = askSubjectId(subjectManager, topicManager);
                if (!subjectId.empty()) listRecommendations(subjectManager, topicManager, subjectId);
                break;
            }
            case 0:
                running = false;
                std::cout << "Exiting STUDYWISE. JSON records remain stored in subjects.json and topics.json.\n";
                break;
        }
    }
    return 0;
}
