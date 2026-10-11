#include "bookmgmt/Budget.h"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
// Every category, in the order Budget::print() lists them.
const ResourceCategory kAllCategories[] = {ResourceCategory::Book,
                                           ResourceCategory::ElectronicResource,
                                           ResourceCategory::Journal,
                                           ResourceCategory::EBook,
                                           ResourceCategory::AudioBook,
                                           ResourceCategory::Thesis};
}

Budget::Budget(Money total) : total_(total), spent_(Money::fromMinor(0, total.currencyCode())) {
    if (total_.isNegative()) throw std::invalid_argument("budget must not be negative");
}

void Budget::setQuota(ResourceCategory c, Quota q) {
    if (q.maxUnits < 0 || q.maxSpend.isNegative() ||
        (q.maxTitles && *q.maxTitles < 0))
        throw std::invalid_argument("quota limits must not be negative");
    if (q.maxSpend.currencyCode() != total_.currencyCode())
        throw std::invalid_argument("quota currency must match budget currency");
    quotas_[c] = q;
    if (usage_[c].units == 0 && usage_[c].spent.isZero())
        usage_[c].spent = Money::fromMinor(0, total_.currencyCode());
}

void Budget::removeQuota(ResourceCategory c) { quotas_.erase(c); }

std::optional<Quota> Budget::quotaFor(ResourceCategory c) const {
    auto it = quotas_.find(c);
    if (it == quotas_.end()) return std::nullopt;
    return it->second;
}

Usage Budget::usageFor(ResourceCategory c) const {
    auto it = usage_.find(c);
    if (it != usage_.end()) return it->second;
    Usage empty;
    empty.spent = Money::fromMinor(0, total_.currencyCode());
    return empty;
}

std::optional<int> Budget::unitsRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxUnits - usageFor(c).units;
}

std::optional<Money> Budget::spendRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxSpend - usageFor(c).spent;
}

std::optional<int> Budget::titlesRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q || !q->maxTitles) return std::nullopt;
    return *q->maxTitles - usageFor(c).titles;
}

Budget::Failure Budget::evaluate(ResourceCategory c, int units, Money cost,
                                 const std::string& titleId, std::string& why) const {
    if (units <= 0) {
        why = "quantity must be positive";
        return Failure::BadInput;
    }
    if (cost.isNegative()) {
        why = "cost must not be negative";
        return Failure::BadInput;
    }
    if (auto left = unitsRemaining(c); left && units > *left) {
        why = std::string(categoryName(c)) + " unit quota exceeded: requested " +
              std::to_string(units) + ", " + std::to_string(*left) + " remaining";
        return Failure::Quota;
    }
    if (auto left = titlesRemaining(c); left) {
        if (titleId.empty()) {
            why = "title ID required when a title quota is configured";
            return Failure::BadInput;
        }
        const auto it = purchasedTitles_.find(c);
        const bool alreadyPurchased = it != purchasedTitles_.end() &&
                                      it->second.find(titleId) != it->second.end() &&
                                      it->second.at(titleId) > 0;
        if (!alreadyPurchased && *left <= 0) {
            why = std::string(categoryName(c)) + " title quota exceeded: no title slots remaining";
            return Failure::Quota;
        }
    }
    if (auto left = spendRemaining(c); left && cost > *left) {
        why = std::string(categoryName(c)) + " spend quota exceeded: cost " +
              cost.toString() + ", " + left->toString() + " remaining";
        return Failure::Quota;
    }
    if (cost > remaining()) {
        why = "overall budget exceeded: cost " + cost.toString() + ", " +
              remaining().toString() + " remaining";
        return Failure::Overall;
    }
    why.clear();
    return Failure::None;
}

std::string Budget::check(ResourceCategory c, int units, Money cost,
                          const std::string& titleId) const {
    std::string why;
    evaluate(c, units, cost, titleId, why);
    return why;
}

void Budget::commit(ResourceCategory c, int units, Money cost,
                    const std::string& titleId) {
    std::string why;
    switch (evaluate(c, units, cost, titleId, why)) {
        case Failure::None: break;
        case Failure::BadInput: throw std::invalid_argument(why);
        case Failure::Quota: throw QuotaExceededError(why);
        case Failure::Overall: throw BudgetExceededError(why);
    }
    Usage& u = usage_[c];
    if (u.units == 0 && u.spent.isZero()) u.spent = Money::fromMinor(0, cost.currencyCode());
    u.units += units;
    u.spent += cost;
    if (!titleId.empty()) {
        int& references = purchasedTitles_[c][titleId];
        if (references++ == 0) ++u.titles;
    }
    spent_ += cost;
}

void Budget::refund(ResourceCategory c, int units, Money cost, const std::string& titleId) {
    if (units <= 0 || cost.isNegative()) throw std::invalid_argument("invalid refund");
    Usage& u = usage_[c];
    if (u.units == 0 && u.spent.isZero()) u.spent = Money::fromMinor(0, cost.currencyCode());
    if (units > u.units || cost > u.spent || cost > spent_)
        throw std::invalid_argument("refund exceeds recorded usage");
    u.units -= units;
    u.spent -= cost;
    spent_ -= cost;
    if (!titleId.empty()) {
        auto categoryIt = purchasedTitles_.find(c);
        if (categoryIt != purchasedTitles_.end()) {
            auto titleIt = categoryIt->second.find(titleId);
            if (titleIt != categoryIt->second.end()) {
                if (--titleIt->second <= 0) {
                    categoryIt->second.erase(titleIt);
                    --u.titles;
                }
            }
        }
    }
}

Budget Budget::createNextYear(Money newAllocation, double carryForwardPercent) const {
    if (!std::isfinite(carryForwardPercent) || carryForwardPercent < 0.0 ||
        carryForwardPercent > 100.0 || newAllocation.isNegative())
        throw std::invalid_argument("allocation must be non-negative and carry percentage in 0..100");
    const auto carry = static_cast<std::int64_t>(std::llround(
        static_cast<long double>(remaining().minorUnits()) * carryForwardPercent / 100.0L));
    Budget next(newAllocation + Money::fromMinor(carry, remaining().currencyCode()));
    for (const auto& [category, quota] : quotas_) next.setQuota(category, quota);
    return next;
}

void Budget::print(std::ostream& os) const {
    os << "Budget: total " << total_ << ", spent " << spent_ << ", remaining "
       << remaining() << "\n";
    os << std::left << std::setw(22) << "  Category" << std::setw(16) << "Units used/max"
       << std::setw(16) << "Titles used/max" << "Spend used/max\n";
    for (ResourceCategory c : kAllCategories) {
        const Usage u = usageFor(c);
        const auto q = quotaFor(c);
        const std::string units =
            std::to_string(u.units) + "/" + (q ? std::to_string(q->maxUnits) : "-");
        const std::string titles = std::to_string(u.titles) + "/" +
            (q && q->maxTitles ? std::to_string(*q->maxTitles) : "-");
        const std::string spend =
            u.spent.toString() + "/" + (q ? q->maxSpend.toString() : "-");
        os << "  " << std::setw(20) << categoryName(c) << std::setw(16) << units
           << std::setw(16) << titles << spend << "\n";
    }
}

}  // namespace bookmgmt
