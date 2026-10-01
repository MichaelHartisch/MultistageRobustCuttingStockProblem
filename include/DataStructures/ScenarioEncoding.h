#ifndef SCENARIO_ENCODING_H
#define SCENARIO_ENCODING_H

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ScenarioEncoding {

inline const std::string& alphabet() {
    static const std::string value = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    return value;
}

inline char encodeItem(int item) {
    const auto& symbols = alphabet();
    if (item < 0 || static_cast<std::size_t>(item) >= symbols.size()) {
        throw std::out_of_range("Item index cannot be encoded in one scenario character: " + std::to_string(item));
    }
    return symbols[static_cast<std::size_t>(item)];
}

inline std::string itemToken(int item) {
    return std::string(1, encodeItem(item));
}

inline int decodeItem(char symbol) {
    const auto& symbols = alphabet();
    const std::size_t pos = symbols.find(symbol);
    if (pos == std::string::npos) {
        throw std::invalid_argument(std::string("Invalid scenario symbol: ") + symbol);
    }
    return static_cast<int>(pos);
}

inline bool scenarioItemEquals(const std::string& scenario, std::size_t position, int item) {
    if (position >= scenario.size()) {
        throw std::out_of_range("Scenario position out of range");
    }
    return scenario[position] == encodeItem(item);
}

inline std::string encodePartition(const std::vector<int>& partition) {
    std::string scenario;
    for (std::size_t item = 0; item < partition.size(); ++item) {
        if (partition[item] < 0) {
            throw std::invalid_argument("Cannot encode a partition with negative item counts");
        }
        scenario.append(static_cast<std::size_t>(partition[item]), encodeItem(static_cast<int>(item)));
    }
    return scenario;
}

inline void validateNumberOfItems(int numberOfItems) {
    if (numberOfItems <= 0) {
        throw std::invalid_argument("Number of item types must be positive");
    }
    if (static_cast<std::size_t>(numberOfItems) > alphabet().size()) {
        throw std::invalid_argument("Too many item types for compact scenario encoding. Maximum supported item types: " + std::to_string(alphabet().size()));
    }
}

} // namespace ScenarioEncoding

#endif // SCENARIO_ENCODING_H
