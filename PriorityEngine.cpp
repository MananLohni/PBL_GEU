#include "PriorityEngine.hpp"
#include "MaxHeap.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <unordered_map>

namespace {
bool parseDate(const std::string& value, int& year, int& month, int& day) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i != 4 && i != 7 && (value[i] < '0' || value[i] > '9')) return false;
    }
    year = std::stoi(value.substr(0, 4));
    month = std::stoi(value.substr(5, 2));
    day = std::stoi(value.substr(8, 2));
    return Subject::isValidDate(value);
}

long long civilDays(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned adjustedMonth = static_cast<unsigned>(static_cast<int>(month) + (month > 2 ? -3 : 9));
    const unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return static_cast<long long>(era) * 146097 + static_cast<long long>(dayOfEra);
}

std::string localToday() {
    const std::time_t now = std::time(nullptr);
    // std::localtime is available in older MinGW GCC versions, including GCC 6.
    // This console application calls the function synchronously, so copying the
    // returned tm value immediately is sufficient here.
    std::tm local{};
    std::tm* localPtr = std::localtime(&now);
    if (localPtr == nullptr) {
        return std::string();
    }
    local = *localPtr;
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
    return buffer;
}

double clampPercent(double value) {
    if (!std::isfinite(value)) return 0.0;
    return std::max(0.0, std::min(100.0, value));
}
}

bool RecommendationHigherPriority::operator()(const TopicRecommendation& a,
                                               const TopicRecommendation& b) const {
    if (a.breakdown.score != b.breakdown.score) return a.breakdown.score > b.breakdown.score;
    if (a.topic.getId() != b.topic.getId()) return a.topic.getId() < b.topic.getId();
    return a.topic.getName() < b.topic.getName();
}

int PriorityEngine::examUrgency(const std::string& examDate, const std::string& today,
                                int& daysUntilExam, bool& validDate) {
    int ey, em, ed, ty, tm, td;
    const std::string effectiveToday = today.empty() ? localToday() : today;
    if (!parseDate(examDate, ey, em, ed) || !parseDate(effectiveToday, ty, tm, td)) {
        daysUntilExam = 0;
        validDate = false;
        return 0;
    }
    const long long delta = civilDays(ey, static_cast<unsigned>(em), static_cast<unsigned>(ed)) -
                            civilDays(ty, static_cast<unsigned>(tm), static_cast<unsigned>(td));
    validDate = true;
    daysUntilExam = static_cast<int>(delta);
    if (delta < 0) return 0;
    if (delta <= 3) return 100;
    if (delta <= 7) return 85;
    if (delta <= 14) return 65;
    if (delta <= 30) return 40;
    if (delta <= 60) return 20;
    return 10;
}

double PriorityEngine::normalizeRating(int value, int minimum, int maximum) {
    if (maximum <= minimum || value < minimum || value > maximum) return 0.0;
    return (static_cast<double>(value - minimum) / (maximum - minimum)) * 100.0;
}

std::vector<TopicRecommendation> PriorityEngine::recommend(
    const std::vector<Subject>& subjects, const std::vector<Topic>& topics,
    const std::string& subjectId, const std::string& today) {
    std::unordered_map<std::string, const Subject*> subjectById;
    for (const auto& subject : subjects) subjectById[subject.getId()] = &subject;

    // Workload is the sum of unfinished hours weighted by remaining syllabus.
    std::unordered_map<std::string, double> workloads;
    for (const auto& topic : topics) {
        const double remaining = topic.getSyllabusRemaining();
        if (topic.getStatus() == "COMPLETED" || !std::isfinite(remaining) || remaining <= 0.0) continue;
        if (subjectById.find(topic.getSubjectId()) == subjectById.end()) continue;
        const double hours = std::isfinite(topic.getEstimatedHours()) && topic.getEstimatedHours() > 0.0
            ? topic.getEstimatedHours() : 0.0;
        workloads[topic.getSubjectId()] += hours * clampPercent(topic.getSyllabusRemaining()) / 100.0;
    }
    double maxWorkload = 0.0;
    for (const auto& item : workloads) maxWorkload = std::max(maxWorkload, item.second);

    MaxHeap<TopicRecommendation, RecommendationHigherPriority> heap;
    for (const auto& topic : topics) {
        const double remaining = topic.getSyllabusRemaining();
        if (topic.getStatus() == "COMPLETED" || !std::isfinite(remaining) || remaining <= 0.0) continue;
        if (!subjectId.empty() && topic.getSubjectId() != subjectId) continue;
        const auto subjectIt = subjectById.find(topic.getSubjectId());
        if (subjectIt == subjectById.end()) continue;

        const Subject& subject = *subjectIt->second;
        TopicRecommendation item;
        item.topic = topic;
        item.subjectName = subject.getName();
        item.examDate = subject.getExamDate();
        item.breakdown.examUrgency = examUrgency(subject.getExamDate(), today,
            item.breakdown.daysUntilExam, item.breakdown.hasValidExamDate);
        item.breakdown.syllabusRemaining = clampPercent(topic.getSyllabusRemaining());
        item.breakdown.academicImportance = normalizeRating(topic.getAcademicImportance());
        item.breakdown.difficulty = normalizeRating(topic.getDifficulty());
        const double workload = workloads[topic.getSubjectId()];
        item.breakdown.subjectWorkload = maxWorkload > 0.0
            ? (workload / maxWorkload) * 100.0 : 0.0;
        item.breakdown.score =
            0.30 * item.breakdown.examUrgency +
            0.25 * item.breakdown.syllabusRemaining +
            0.20 * item.breakdown.academicImportance +
            0.10 * item.breakdown.difficulty +
            0.15 * item.breakdown.subjectWorkload;
        item.topic.setPriority(item.breakdown.score);
        heap.insert(item);
    }

    std::vector<TopicRecommendation> ordered;
    ordered.reserve(heap.size());
    while (!heap.empty()) ordered.push_back(heap.extractMax());
    return ordered;
}
