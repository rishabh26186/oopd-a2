#include "bookmgmt/AudioBook.h"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace bookmgmt {

AudioBook::AudioBook(std::string id, std::string title, std::vector<std::string> authors,
                     std::string isbn, std::string publisher, int year, Money unitPrice,
                     std::string narrator, int durationMinutes, int edition)
    : Book(std::move(id), std::move(title), std::move(authors), std::move(isbn),
           std::move(publisher), year, unitPrice, edition),
      narrator_(std::move(narrator)), durationMinutes_(durationMinutes) {
    if (narrator_.empty()) throw std::invalid_argument("narrator must not be empty");
    if (durationMinutes_ <= 0) throw std::invalid_argument("duration must be positive");
}

Money AudioBook::costFor(int quantity) const {
    // Audio editions are not print items; retain the ordinary per-copy price.
    return Resource::costFor(quantity);
}

void AudioBook::printDetails(std::ostream& os) const {
    Book::printDetails(os);
    os << "  narrator: " << narrator_ << "\n"
       << "  duration minutes: " << durationMinutes_ << "\n";
}

}  // namespace bookmgmt
