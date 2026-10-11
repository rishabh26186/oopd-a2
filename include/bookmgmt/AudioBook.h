#pragma once

#include <string>
#include <vector>

#include "bookmgmt/Book.h"

namespace bookmgmt {

// An audiobook is a book-format work, so deriving from Book reuses its authors,
// ISBN, edition, and binding metadata. It has its own category for quota tracking.
class AudioBook : public Book {
public:
    AudioBook(std::string id, std::string title, std::vector<std::string> authors,
              std::string isbn, std::string publisher, int year, Money unitPrice,
              std::string narrator, int durationMinutes, int edition = 1);

    const std::string& narrator() const { return narrator_; }
    int durationMinutes() const { return durationMinutes_; }
    ResourceCategory category() const override { return ResourceCategory::AudioBook; }
    // Audio editions are not print items, so they do not receive the print bulk discount.
    Money costFor(int quantity) const override;

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::string narrator_;
    int durationMinutes_;
};

}  // namespace bookmgmt
