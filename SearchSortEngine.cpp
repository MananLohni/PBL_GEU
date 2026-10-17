#include "Optional.hpp"
#include "SearchSortEngine.hpp"
#include <cctype>
#include <algorithm>
#include <iostream>

// ==========================================
// STRING NORMALIZATION UTILITIES
// ==========================================

std::string SearchSortEngine::toLowerCase(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

// Case-insensitive substring algorithm
bool SearchSortEngine::containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (needle.length() > haystack.length()) return false;

    std::string lowerHaystack = toLowerCase(haystack);
    std::string lowerNeedle = toLowerCase(needle);

    // Explicit substring matching
    size_t n = lowerHaystack.length();
    size_t m = lowerNeedle.length();

    for (size_t i = 0; i <= n - m; ++i) {
        size_t j = 0;
        while (j < m && lowerHaystack[i + j] == lowerNeedle[j]) {
            ++j;
        }
        if (j == m) return true;
    }

    return false;
}

// ==========================================
// 1. LINEAR SEARCH IMPLEMENTATION - O(n)
// ==========================================

std::vector<Topic> SearchSortEngine::linearSearchTopics(
    const std::vector<Topic>& topics,
    const std::string& query,
    const std::function<std::string(const std::string&)>& getSubjectNameById
) {
    std::vector<Topic> results;
    std::string trimmedQuery = query;

    // Linear sequential traversal through the dataset
    for (size_t i = 0; i < topics.size(); ++i) {
        const Topic& t = topics[i];
        bool matched = false;

        // Check 1: Topic name match
        if (containsIgnoreCase(t.getName(), trimmedQuery)) {
            matched = true;
        }

        // Check 2: Topic status match (e.g. "IN_PROGRESS", "COMPLETED")
        if (!matched && containsIgnoreCase(t.getStatus(), trimmedQuery)) {
            matched = true;
        }

        // Check 3: Associated subject name match
        if (!matched && getSubjectNameById) {
            std::string subName = getSubjectNameById(t.getSubjectId());
            if (!subName.empty() && containsIgnoreCase(subName, trimmedQuery)) {
                matched = true;
            }
        }

        if (matched) {
            results.push_back(t);
        }
    }

    return results;
}

std::vector<Subject> SearchSortEngine::linearSearchSubjects(
    const std::vector<Subject>& subjects,
    const std::string& query
) {
    std::vector<Subject> results;

    // Linear sequential traversal through the subjects dataset
    for (size_t i = 0; i < subjects.size(); ++i) {
        const Subject& s = subjects[i];
        bool matched = false;

        if (containsIgnoreCase(s.getName(), query)) matched = true;
        else if (containsIgnoreCase(s.getCode(), query)) matched = true;
        else if (containsIgnoreCase(s.getSemester(), query)) matched = true;
        else if (containsIgnoreCase(s.getExamDate(), query)) matched = true;

        if (matched) {
            results.push_back(s);
        }
    }

    return results;
}

// ==========================================
// 2. MERGE SORT IMPLEMENTATION - O(n log n)
// ==========================================

template<typename T, typename Comparator>
void SearchSortEngine::merge(std::vector<T>& arr, int left, int mid, int right, Comparator comp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // Create temporary work buffers for the left and right halves
    std::vector<T> leftArr;
    std::vector<T> rightArr;
    leftArr.reserve(n1);
    rightArr.reserve(n2);

    for (int i = 0; i < n1; ++i) {
        leftArr.push_back(arr[left + i]);
    }
    for (int j = 0; j < n2; ++j) {
        rightArr.push_back(arr[mid + 1 + j]);
    }

    // Merge the temporary buffers back into arr[left..right]
    int i = 0;
    int j = 0;
    int k = left;

    while (i < n1 && j < n2) {
        if (comp(leftArr[i], rightArr[j])) {
            arr[k++] = std::move(leftArr[i++]);
        } else {
            arr[k++] = std::move(rightArr[j++]);
        }
    }

    // Copy any remaining elements of leftArr
    while (i < n1) {
        arr[k++] = std::move(leftArr[i++]);
    }

    // Copy any remaining elements of rightArr
    while (j < n2) {
        arr[k++] = std::move(rightArr[j++]);
    }
}

template<typename T, typename Comparator>
void SearchSortEngine::mergeSortRecursive(std::vector<T>& arr, int left, int right, Comparator comp) {
    if (left < right) {
        // Find midpoint (avoids overflow)
        int mid = left + (right - left) / 2;

        // Divide: Sort first and second halves
        mergeSortRecursive(arr, left, mid, comp);
        mergeSortRecursive(arr, mid + 1, right, comp);

        // Conquer: Merge sorted halves
        merge(arr, left, mid, right, comp);
    }
}

std::vector<Topic> SearchSortEngine::sortTopics(
    const std::vector<Topic>& topics,
    const std::string& criteria,
    bool ascending
) {
    if (topics.size() <= 1) return topics;

    std::vector<Topic> sorted = topics;

    // Construct comparator based on requested criteria
    auto comp = [criteria, ascending](const Topic& a, const Topic& b) -> bool {
        bool less = false;
        if (criteria == "difficulty") {
            if (a.getDifficulty() != b.getDifficulty()) {
                less = a.getDifficulty() < b.getDifficulty();
            } else {
                less = a.getName() < b.getName();
            }
        } else if (criteria == "syllabusRemaining") {
            if (a.getSyllabusRemaining() != b.getSyllabusRemaining()) {
                less = a.getSyllabusRemaining() < b.getSyllabusRemaining();
            } else {
                less = a.getName() < b.getName();
            }
        } else if (criteria == "academicImportance") {
            if (a.getAcademicImportance() != b.getAcademicImportance()) {
                less = a.getAcademicImportance() < b.getAcademicImportance();
            } else {
                less = a.getName() < b.getName();
            }
        } else if (criteria == "estimatedHours") {
            if (a.getEstimatedHours() != b.getEstimatedHours()) {
                less = a.getEstimatedHours() < b.getEstimatedHours();
            } else {
                less = a.getName() < b.getName();
            }
        } else {
            // Default: "name"
            less = toLowerCase(a.getName()) < toLowerCase(b.getName());
        }

        return ascending ? less : !less;
    };

    mergeSortRecursive(sorted, 0, static_cast<int>(sorted.size()) - 1, comp);
    return sorted;
}

std::vector<Subject> SearchSortEngine::sortSubjects(
    const std::vector<Subject>& subjects,
    const std::string& criteria,
    bool ascending
) {
    if (subjects.size() <= 1) return subjects;

    std::vector<Subject> sorted = subjects;

    auto comp = [criteria, ascending](const Subject& a, const Subject& b) -> bool {
        bool less = false;
        if (criteria == "code") {
            less = toLowerCase(a.getCode()) < toLowerCase(b.getCode());
        } else if (criteria == "semester") {
            less = toLowerCase(a.getSemester()) < toLowerCase(b.getSemester());
        } else if (criteria == "examDate") {
            if (a.getExamDate() != b.getExamDate()) {
                less = a.getExamDate() < b.getExamDate();
            } else {
                less = toLowerCase(a.getName()) < toLowerCase(b.getName());
            }
        } else {
            // Default: "name"
            less = toLowerCase(a.getName()) < toLowerCase(b.getName());
        }

        return ascending ? less : !less;
    };

    mergeSortRecursive(sorted, 0, static_cast<int>(sorted.size()) - 1, comp);
    return sorted;
}

// ==========================================
// 3. BINARY SEARCH IMPLEMENTATION - O(log n)
// ==========================================

Optional<Topic> SearchSortEngine::binarySearchTopicExact(
    const std::vector<Topic>& topics,
    const std::string& targetName
) {
    if (topics.empty() || targetName.empty()) {
        return nullopt;
    }

    // Step 1: Pre-condition of Binary Search — the collection must be sorted.
    // We sort by normalized Topic Name using our Merge Sort engine in O(n log n).
    std::vector<Topic> sortedTopics = sortTopics(topics, "name", true);

    // Step 2: Binary Search in O(log n)
    std::string targetNormalized = toLowerCase(targetName);
    // Trim leading/trailing whitespace
    auto start = targetNormalized.find_first_not_of(" \t\n\r");
    if (start != std::string::npos) {
        auto end = targetNormalized.find_last_not_of(" \t\n\r");
        targetNormalized = targetNormalized.substr(start, end - start + 1);
    }

    int low = 0;
    int high = static_cast<int>(sortedTopics.size()) - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        std::string midName = toLowerCase(sortedTopics[mid].getName());

        if (midName == targetNormalized) {
            // Exact match found!
            return sortedTopics[mid];
        } else if (midName < targetNormalized) {
            // Target lies in the upper half
            low = mid + 1;
        } else {
            // Target lies in the lower half
            high = mid - 1;
        }
    }

    // Target not found in the search space
    return nullopt;
}
