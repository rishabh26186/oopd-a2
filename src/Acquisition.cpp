#include "bookmgmt/Acquisition.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>

#include "bookmgmt/AudioBook.h"
#include "bookmgmt/Book.h"
#include "bookmgmt/EBook.h"
#include "bookmgmt/ElectronicResource.h"
#include "bookmgmt/Exceptions.h"
#include "bookmgmt/Journal.h"
#include "bookmgmt/Thesis.h"

namespace bookmgmt {

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget)
    : catalog_(catalog), budget_(budget) {}

Budget& AcquisitionManager::budgetFor(const std::string& department) {
    if (department.empty() || department == "General") return budget_;
    return departmentBudget(department);
}

const Budget& AcquisitionManager::budgetFor(const std::string& department) const {
    if (department.empty() || department == "General") return budget_;
    return departmentBudget(department);
}

void AcquisitionManager::registerDepartment(std::string name, Budget& budget) {
    if (name.empty() || name == "General") throw std::invalid_argument("department name is empty or reserved");
    if (!departments_.emplace(std::move(name), &budget).second)
        throw std::invalid_argument("department already registered");
}

Budget& AcquisitionManager::departmentBudget(const std::string& department) {
    auto it = departments_.find(department);
    if (it == departments_.end()) throw std::invalid_argument("unknown department: " + department);
    return *it->second;
}

const Budget& AcquisitionManager::departmentBudget(const std::string& department) const {
    auto it = departments_.find(department);
    if (it == departments_.end()) throw std::invalid_argument("unknown department: " + department);
    return *it->second;
}

Money AcquisitionManager::quoteBeforeTax(const std::string& id, int quantity) const {
    const Resource& resource = catalog_.get(id);
    return vendorCost(resource, quantity, catalog_.cheapestVendor(id));
}

Money AcquisitionManager::vendorCost(const Resource& resource, int quantity,
                                     const VendorOffer& vendor) const {
    if (quantity <= 0) throw std::invalid_argument("quantity must be positive");
    if (vendor.unitPrice == resource.unitPrice()) return resource.costFor(quantity);
    if (const auto* audio = dynamic_cast<const AudioBook*>(&resource)) {
        (void)audio;
        return vendor.unitPrice * quantity;
    }
    if (const auto* journal = dynamic_cast<const Journal*>(&resource)) {
        Money total = vendor.unitPrice * quantity * journal->subscriptionYears();
        if (quantity >= 10) total -= Money::fromMinor(total.minorUnits() / 10, total.currencyCode());
        return total;
    }
    if (const auto* book = dynamic_cast<const Book*>(&resource)) {
        Money total = vendor.unitPrice * quantity;
        if (book->binding() == Binding::Hardcover)
            total += Money::fromMinor(total.minorUnits() / 5, total.currencyCode());
        if (quantity >= 10) total -= Money::fromMinor(total.minorUnits() / 10, total.currencyCode());
        return total;
    }
    if (const auto* digital = dynamic_cast<const ElectronicResource*>(&resource)) {
        const int fullSeats = std::min(quantity, 50);
        const int halfSeats = std::max(0, quantity - 50);
        Money total = vendor.unitPrice * fullSeats +
                      Money::fromMinor(vendor.unitPrice.minorUnits() / 2,
                                       vendor.unitPrice.currencyCode()) * halfSeats;
        total += digital->platformFee();
        return total;
    }
    return vendor.unitPrice * quantity;
}

Money AcquisitionManager::taxFor(const Resource& resource, Money preTaxCost) const {
    const double rate = resource.isDigital() ? electronicTaxPercent_ : printTaxPercent_;
    const long double rawMinor = static_cast<long double>(preTaxCost.minorUnits()) *
                                 static_cast<long double>(rate) / 100.0L;
    if (rawMinor > static_cast<long double>(std::numeric_limits<std::int64_t>::max()) ||
        rawMinor < static_cast<long double>(std::numeric_limits<std::int64_t>::min()))
        throw std::overflow_error("calculated tax is out of range");
    return Money::fromMinor(static_cast<std::int64_t>(std::llround(rawMinor)), preTaxCost.currencyCode());
}

Money AcquisitionManager::quote(const std::string& id, int quantity) const {
    const Resource& resource = catalog_.get(id);
    const Money preTax = vendorCost(resource, quantity, catalog_.cheapestVendor(id));
    return preTax + taxFor(resource, preTax);
}

void AcquisitionManager::setTaxRates(double printPercent, double electronicPercent) {
    if (!std::isfinite(printPercent) || !std::isfinite(electronicPercent) ||
        printPercent < 0.0 || electronicPercent < 0.0)
        throw std::invalid_argument("tax rates must be finite and non-negative");
    printTaxPercent_ = printPercent;
    electronicTaxPercent_ = electronicPercent;
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason) const {
    return canPurchase(id, quantity, "General", reason);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     const std::string& department,
                                     std::string* reason) const {
    std::string why;
    if (const Resource* r = catalog_.find(id)) {
        if (quantity <= 0) why = "quantity must be positive";
        else {
            try {
                const Money preTax = vendorCost(*r, quantity, catalog_.cheapestVendor(id));
                why = budgetFor(department).check(r->category(), quantity,
                                                 preTax + taxFor(*r, preTax), r->id());
            } catch (const std::exception& e) { why = e.what(); }
        }
    } else why = "resource not found: " + id;
    if (reason) *reason = why;
    return why.empty();
}

PurchaseRecord& AcquisitionManager::record(const Resource* r, const std::string& id,
                                           int qty, Money preTaxCost, Money tax,
                                           Money totalCost, bool approved,
                                           std::string reason) {
    history_.push_back(PurchaseRecord{nextOrderNo_++, id,
        r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book, qty, preTaxCost, tax, totalCost,
        approved, std::move(reason)});
    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity) {
    return purchase(id, quantity, "General");
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity,
                                                   const std::string& department) {
    const Resource& resource = catalog_.get(id);
    const VendorOffer vendor = catalog_.cheapestVendor(id);
    const Money preTax = vendorCost(resource, quantity, vendor);
    const Money tax = taxFor(resource, preTax);
    const Money total = preTax + tax;
    Budget& target = budgetFor(department);
    target.commit(resource.category(), quantity, total, resource.id());
    catalog_.addHoldings(id, quantity);
    PurchaseRecord& rec = record(&resource, id, quantity, preTax, tax, total, true, {});
    rec.department = department.empty() ? "General" : department;
    rec.vendor = vendor.vendor;
    orderDepartments_[rec.orderNo] = rec.department;
    return rec;
}

const PurchaseRecord& AcquisitionManager::cancel(int orderNo) {
    auto it = std::find_if(history_.begin(), history_.end(), [orderNo](const PurchaseRecord& r) {
        return r.orderNo == orderNo && !r.isCancellation;
    });
    if (it == history_.end() || !it->approved) throw std::invalid_argument("approved order not found");
    if (cancelledOrders_.count(orderNo)) throw std::invalid_argument("order already cancelled");
    PurchaseRecord original = *it;
    budgetFor(original.department).refund(original.category, original.quantity,
                                          original.cost, original.resourceId);
    catalog_.addHoldings(original.resourceId, -original.quantity);
    cancelledOrders_.insert(orderNo);
    PurchaseRecord cancellation = original;
    cancellation.orderNo = nextOrderNo_++;
    cancellation.reason = "cancellation of order #" + std::to_string(orderNo);
    cancellation.isCancellation = true;
    cancellation.referenceOrderNo = orderNo;
    history_.push_back(std::move(cancellation));
    return history_.back();
}

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs, bool allOrNothing) {
    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());
    if (allOrNothing) {
        std::map<std::string, Budget> simulated;
        simulated.emplace("General", budget_);
        for (const auto& [name, ptr] : departments_) simulated.emplace(name, *ptr);
        std::vector<std::string> reasons;
        reasons.reserve(reqs.size());
        bool failed = false;
        for (const auto& req : reqs) {
            std::string dept = req.department.empty() ? "General" : req.department;
            std::string why;
            const Resource* resource = catalog_.find(req.resourceId);
            if (!resource) why = "resource not found: " + req.resourceId;
            else if (req.quantity <= 0) why = "quantity must be positive";
            else if (!simulated.count(dept)) why = "unknown department: " + dept;
            else {
                const Money preTax = vendorCost(*resource, req.quantity, catalog_.cheapestVendor(req.resourceId));
                const Money total = preTax + taxFor(*resource, preTax);
                why = simulated.at(dept).check(resource->category(), req.quantity, total, resource->id());
                if (why.empty()) simulated.at(dept).commit(resource->category(), req.quantity, total, resource->id());
            }
            if (!why.empty()) failed = true;
            reasons.push_back(std::move(why));
        }
        if (failed) {
            for (std::size_t i = 0; i < reqs.size(); ++i) {
                const auto& req = reqs[i];
                const Resource* resource = catalog_.find(req.resourceId);
                std::string why = reasons[i].empty() ? "batch aborted because another request would be rejected" : reasons[i];
                Money preTax = Money::fromMinor(0, budget_.total().currencyCode());
                Money tax = preTax;
                Money cost = preTax;
                if (resource && req.quantity > 0) {
                    try {
                        preTax = vendorCost(*resource, req.quantity, catalog_.cheapestVendor(req.resourceId));
                        tax = taxFor(*resource, preTax);
                        cost = preTax + tax;
                    } catch (const std::exception&) { }
                }
                PurchaseRecord& rec = record(resource, req.resourceId, req.quantity,
                    preTax, tax, cost, false, why);
                if (resource) rec.vendor = catalog_.cheapestVendor(req.resourceId).vendor;
                rec.department = req.department.empty() ? "General" : req.department;
                results.push_back(rec);
            }
            return results;
        }
    }
    for (const auto& req : reqs) {
        const Resource* resource = catalog_.find(req.resourceId);
        std::string why;
        Money preTax = Money::fromMinor(0, budget_.total().currencyCode());
        Money tax = preTax;
        Money cost = preTax;
        VendorOffer vendor{"", preTax};
        if (!resource) why = "resource not found: " + req.resourceId;
        else if (req.quantity <= 0) why = "quantity must be positive";
        else {
            vendor = catalog_.cheapestVendor(req.resourceId);
            preTax = vendorCost(*resource, req.quantity, vendor);
            tax = taxFor(*resource, preTax);
            cost = preTax + tax;
            (void)canPurchase(req.resourceId, req.quantity,
                              req.department.empty() ? "General" : req.department, &why);
        }
        if (why.empty()) results.push_back(purchase(req.resourceId, req.quantity,
                                                    req.department.empty() ? "General" : req.department));
        else {
            PurchaseRecord& rec = record(resource, req.resourceId, req.quantity,
                preTax, tax, cost, false, why);
            rec.department = req.department.empty() ? "General" : req.department;
            rec.vendor = vendor.vendor;
            results.push_back(rec);
        }
    }
    return results;
}

Money AcquisitionManager::totalSpent() const {
    Money sum = Money::fromMinor(0, budget_.total().currencyCode());
    for (const auto& rec : history_) if (rec.approved) {
        if (rec.isCancellation) sum -= rec.cost; else sum += rec.cost;
    }
    return sum;
}

Money AcquisitionManager::totalBeforeTax() const {
    Money sum = Money::fromMinor(0, budget_.total().currencyCode());
    for (const auto& rec : history_) if (rec.approved) {
        if (rec.isCancellation) sum -= rec.preTaxCost; else sum += rec.preTaxCost;
    }
    return sum;
}

Money AcquisitionManager::totalTax() const {
    Money sum = Money::fromMinor(0, budget_.total().currencyCode());
    for (const auto& rec : history_) if (rec.approved) {
        if (rec.isCancellation) sum -= rec.tax; else sum += rec.tax;
    }
    return sum;
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " records)\n";
    for (const auto& rec : history_) {
        const char* status = rec.isCancellation ? "CANCELLED" : (rec.approved ? "APPROVED" : "REJECTED");
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " " << status << "  "
           << std::setw(6) << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << rec.title << " [" << rec.department << "]";
        if (!rec.vendor.empty()) os << " vendor: " << rec.vendor;
        os << "\n        pre-tax: " << rec.preTaxCost << ", tax: " << rec.tax
           << ", post-tax: " << rec.cost;
        if (!rec.reason.empty()) os << "\n        reason: " << rec.reason;
        os << "\n";
    }
    os << "Total before tax: " << totalBeforeTax() << "\n";
    os << "Total tax: " << totalTax() << "\n";
    os << "Total after tax: " << totalSpent() << "\n";
}

}  // namespace bookmgmt
