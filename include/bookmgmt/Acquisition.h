#pragma once
// AcquisitionManager: turns purchase requests into orders, enforcing the
// Budget's quotas, updating Catalog holdings and keeping an order history.

#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bookmgmt/Budget.h"
#include "bookmgmt/Catalog.h"

namespace bookmgmt {

struct PurchaseRequest {
    std::string resourceId;
    int quantity;  // copies for print, seats for electronic
    std::string department;
};

struct PurchaseRecord {
    int orderNo;
    std::string resourceId;
    std::string title;
    ResourceCategory category;
    int quantity;
    Money preTaxCost;
    Money tax;
    Money cost;  // total including tax
    bool approved;
    std::string reason;  // why it was rejected; empty if approved
    std::string department = "General";
    std::string vendor = "";
    bool isCancellation = false;
    int referenceOrderNo = 0;
};

class AcquisitionManager {
public:
    AcquisitionManager(Catalog& catalog, Budget& budget);

    // Quotes return totals including tax; quoteBeforeTax returns the item price.
    // Both throw NotFoundError for an unknown resource.
    Money quote(const std::string& id, int quantity) const;
    Money quoteBeforeTax(const std::string& id, int quantity) const;

    // Configure percentage rates (e.g. 5.0 means 5%). Defaults are zero.
    // Digital resources use the electronic rate; other resources use the print rate.
    void setTaxRates(double printPercent, double electronicPercent);
    double printTaxRate() const { return printTaxPercent_; }
    double electronicTaxRate() const { return electronicTaxPercent_; }

    // True if the purchase would be approved; if not, `reason` explains why.
    bool canPurchase(const std::string& id, int quantity,
                     std::string* reason = nullptr) const;
    bool canPurchase(const std::string& id, int quantity, const std::string& department,
                     std::string* reason = nullptr) const;
    void registerDepartment(std::string name, Budget& budget);
    Budget& departmentBudget(const std::string& department);
    const Budget& departmentBudget(const std::string& department) const;

    // Buys immediately. Throws NotFoundError, QuotaExceededError,
    // BudgetExceededError or std::invalid_argument. On success the budget
    // and holdings are updated and the record is added to history.
    const PurchaseRecord& purchase(const std::string& id, int quantity);
    const PurchaseRecord& purchase(const std::string& id, int quantity,
                                   const std::string& department);
    const PurchaseRecord& cancel(int orderNo);

    // Processes requests in order; each is approved or rejected on its own
    // (never throws for a rejected request). Every outcome is recorded.
    // EXTENSION POINT: priority ordering, all-or-nothing batches, ...
    std::vector<PurchaseRecord> processBatch(const std::vector<PurchaseRequest>& reqs,
                                             bool allOrNothing = false);

    const std::vector<PurchaseRecord>& history() const { return history_; }
    Money totalSpent() const;  // post-tax total of approved purchases
    Money totalBeforeTax() const;
    Money totalTax() const;

    void printReport(std::ostream& os) const;

private:
    Money taxFor(const Resource& resource, Money preTaxCost) const;
    Money vendorCost(const Resource& resource, int quantity, const VendorOffer& vendor) const;
    Budget& budgetFor(const std::string& department);
    const Budget& budgetFor(const std::string& department) const;
    PurchaseRecord& record(const Resource* r, const std::string& id, int qty,
                           Money preTaxCost, Money tax, Money totalCost,
                           bool approved, std::string reason);

    Catalog& catalog_;
    Budget& budget_;
    std::vector<PurchaseRecord> history_;
    int nextOrderNo_ = 1;
    double printTaxPercent_ = 0.0;
    double electronicTaxPercent_ = 0.0;
    std::map<std::string, Budget*> departments_;
    std::map<int, std::string> orderDepartments_;
    std::set<int> cancelledOrders_;
};

}  // namespace bookmgmt
