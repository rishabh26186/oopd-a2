#include "bookmgmt/Journal.h"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace bookmgmt {

Journal::Journal(std::string id, std::string title, std::string publisher, int year,
                 Money annualSubscriptionPrice, std::string issn, int issuesPerYear,
                 int subscriptionYears)
    : Resource(std::move(id), std::move(title), std::move(publisher), year,
               annualSubscriptionPrice),
      issn_(std::move(issn)),
      issuesPerYear_(issuesPerYear),
      subscriptionYears_(subscriptionYears) {
    if (subscriptionYears_ < 1) {
        throw std::invalid_argument("subscription length must be >= 1 year");
    }
    if (issuesPerYear_ < 1) {
        throw std::invalid_argument("issues per year must be >= 1");
    }
}

Money Journal::costFor(int copies) const {
    requirePositive(copies);
    Money total = unitPrice() * copies * subscriptionYears_;
    // Journals are print items; apply the 10% bulk discount at ten copies.
    if (copies >= 10) {
        total -= Money::fromMinor(total.minorUnits() / 10, total.currencyCode());
    }
    return total;
}

void Journal::printDetails(std::ostream& os) const {
    os << "  issn: " << issn_ << "\n"
       << "  issues per year: " << issuesPerYear_ << "\n"
       << "  subscription years: " << subscriptionYears_ << "\n";
}

}  // namespace bookmgmt
