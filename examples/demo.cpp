// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

int main() {
    Catalog catalog;

    catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                          "978-0132350884", "Prentice Hall", 2008, Money::of(450));
    catalog.emplace<Book>("B002", "The C++ Programming Language",
                          std::vector<std::string>{"Bjarne Stroustrup"}, "978-0321563842",
                          "Addison-Wesley", 2013, Money::of(1200), 4, Binding::Hardcover);
    std::cout << "Hardcover B002 cost for 2 copies: "
              << catalog.get("B002").costFor(2) << "\n\n";
    catalog.emplace<Journal>("J001", "Journal of Computing", "Academic Press", 2025,
                             Money::of(120), "2049-3630", 12, 2);
    catalog.emplace<EBook>("E001", "Modern C++", "Digital Press", 2024,
                           Money::of(25), "https://ebooks.example/modern-cpp",
                           std::vector<std::string>{"A. Author"}, "978-1234567890",
                           EBookFormat::PDF, true, LicenseModel::AnnualSubscription,
                           Money::of(100));
    catalog.emplace<AudioBook>("A001", "The C++ Journey",
                               std::vector<std::string>{"A. Writer"}, "978-audio-001",
                               "Audio Press", 2025, Money::of(30), "N. Narrator", 360);
    catalog.emplace<Thesis>("T001", "Cache Coherence Study", "IIIT Delhi", 2026,
                            "IIIT Delhi", "M.Tech", "Dr. Guide");
    catalog.emplace<ElectronicResource>("R001", "IEEE Xplore Digital Library", "IEEE", 2026,
                                        Money::of(150), "https://ieeexplore.example",
                                        LicenseModel::AnnualSubscription, Money::of(2000));
    catalog.emplace<ElectronicResource>("R002", "MATLAB Campus Licence", "MathWorks", 2026,
                                        Money::of(400), "https://licensing.example/matlab",
                                        LicenseModel::Perpetual);

    std::cout << "=== Catalog ===\n";
    for (const Resource* r : catalog.all()) std::cout << r->summary() << "\n";

    std::cout << "\n=== Details of R001 ===\n" << catalog.get("R001");

    Budget budget(Money::of(20000));
    budget.setQuota(ResourceCategory::Book, {10, Money::of(8000), 2}); // at most 2 distinct book titles
    budget.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(12000)});
    budget.setQuota(ResourceCategory::Journal, {5, Money::of(2000)});
    budget.setQuota(ResourceCategory::EBook, {10, Money::of(1000)});
    budget.setQuota(ResourceCategory::AudioBook, {5, Money::of(500)});
    budget.setQuota(ResourceCategory::Thesis, {5, Money::of(0)});

    AcquisitionManager acq(catalog, budget);

    std::cout << "\n=== Quotes ===\n";
    std::cout << "5 copies of B002  = " << acq.quote("B002", 5) << "\n";
    std::cout << "20 seats of R001  = " << acq.quote("R001", 20) << "  (incl. platform fee)\n";

    acq.processBatch({
        {"J001", 2},   // 480: two years of annual subscriptions\n        {"B001", 4},   // 1800  ok
        {"B002", 5},   // 6000  ok  -> book spend 7800
        {"B001", 1},   // 450   rejected: book spend quota (200 left)
        {"R001", 20},  // 5000  ok
        {"R002", 25},  // 10000 rejected: e-resource unit quota (20 seats left)
        {"R002", 15},  // 6000  ok  -> e-resource spend 11000
        {"R002", 5},   // 2000  rejected: e-resource spend quota (1000 left)
        {"X999", 1},   // rejected: unknown id
    });

    std::cout << "\n=== Acquisition report ===\n";
    acq.printReport(std::cout);

    std::cout << "\n=== Budget (including distinct-title quota usage) ===\n";
    budget.print(std::cout);

    std::cout << "\n=== Holdings ===\n";
    for (const Resource* r : catalog.all())
        std::cout << "  " << r->id() << ": " << catalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    // Direct purchase: errors are reported with exceptions
    std::cout << "\n=== Direct purchase that breaks a quota ===\n";
    try {
        acq.purchase("B002", 1);
    } catch (const QuotaExceededError& e) {
        std::cout << "QuotaExceededError: " << e.what() << "\n";
    }

    std::cout << "\n=== Q8: Cancellation ===\n";
    if (!acq.history().empty() && acq.history().front().approved) {
        const int orderNo = acq.history().front().orderNo;
        acq.cancel(orderNo);
        std::cout << "Cancelled order #" << orderNo << "; history records retained: "
                  << acq.history().size() << "\n";
    }

    std::cout << "\n=== Q9: Department budgets ===\n";
    Budget csBudget(Money::of(1500));
    csBudget.setQuota(ResourceCategory::Book, {5, Money::of(1000), 3});
    acq.registerDepartment("Computer Science", csBudget);
    const auto& departmentOrder = acq.purchase("B001", 1, "Computer Science");
    std::cout << "Order #" << departmentOrder.orderNo << " charged to "
              << departmentOrder.department << " budget: " << csBudget.spent() << "\n";

    std::cout << "\n=== Q10: Year-end rollover ===\n";
    Budget nextYear = budget.createNextYear(Money::of(1000), 25.0);
    std::cout << "Next-year allocation including 25% of this year's unspent balance: "
              << nextYear.total() << "\n";

    std::cout << "\n=== Q11: All-or-nothing batch ===\n";
    const auto atomic = acq.processBatch({{"B001", 1}, {"UNKNOWN", 1}}, true);
    std::cout << "Batch outcomes: " << atomic.size() << "; holdings of B001 remain "
              << catalog.holdings("B001") << " because the batch was aborted.\n";

    std::cout << "\n=== Q12: Cheapest vendor ===\n";
    catalog.addVendor("B001", "Campus Books", Money::of(300));
    catalog.addVendor("B001", "City Books", Money::of(325));
    std::cout << "Cheapest-vendor quote for B001: " << acq.quote("B001", 1) << "\n";
    const auto& vendorOrder = acq.purchase("B001", 1);
    std::cout << "Selected vendor: " << vendorOrder.vendor << "\n";

    std::cout << "\n=== Q13: Catalogue searches ===\n";
    std::cout << "Author search results: " << catalog.searchAuthor("Robert C. Martin").size()
              << "; ISBN search results: " << catalog.searchIdentifier("978-0132350884").size()
              << "; 2020-2026 resources: " << catalog.searchYears(2020, 2026).size() << "\n";

    std::cout << "\n=== Q14: Lending ===\n";
    catalog.borrow("Demo Patron", "B001");
    catalog.openSession("Demo Patron", "R001");
    std::cout << "Copies borrowed: " << catalog.borrowed("Demo Patron", "B001")
              << "; electronic sessions: " << catalog.openSessions("Demo Patron", "R001") << "\n";
    catalog.returnCopy("Demo Patron", "B001");
    catalog.closeSession("Demo Patron", "R001");

    std::cout << "\n=== Q15: Currency-safe money ===\n";
    try {
        std::cout << Money::of(10, 0, "USD") + Money::of(10, 0, "INR") << "\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Mixed currencies rejected: " << e.what() << "\n";
    }
    std::cout << "Q16: tests use standard assert checks (see tests/test_main.cpp).\n";
    return 0;
}
