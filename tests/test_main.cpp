// Minimal self-contained test runner (no external framework needed).

#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

template <typename Exception, typename Callable>
static void expectThrows(Callable&& callable) {
    bool thrown = false;
    try { callable(); } catch (const Exception&) { thrown = true; } catch (...) { }
    assert(thrown && "expected exception type");
}

static void testMoney() {
    assert(Money::of(12, 5).toString() == "12.05");
    assert(Money::of(-3, 50).toString() == "-3.50");
    assert(Money::fromMinor(7).toString() == "0.07");
    assert(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    assert(Money::of(3) * 4 == Money::of(12));
    assert(Money::of(1) < Money::of(2));
    expectThrows<std::invalid_argument>(([&]() { (void)(Money::of(1, 100)); }));
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    assert(b.category() == ResourceCategory::Book);
    assert(!b.isDigital());
    assert(b.costFor(3) == Money::of(300));
    expectThrows<std::invalid_argument>(([&]() { (void)(b.costFor(0)); }));
    assert(joinAuthors(b.authors()) == "A, B and C");

    Book paperback("BP", "Paperback", {"A"}, "isbn-p", "P", 2024,
                   Money::of(100), 1, Binding::Paperback);
    Book hardcover("BH", "Hardcover", {"A"}, "isbn-h", "P", 2024,
                   Money::of(100), 1, Binding::Hardcover);
    assert(paperback.costFor(2) == Money::of(200));
    assert(paperback.costFor(9) == Money::of(900));
    assert(paperback.costFor(10) == Money::of(900));
    assert(hardcover.costFor(1) == Money::of(120));
    assert(hardcover.costFor(3) == Money::of(360));
    assert(hardcover.costFor(10) == Money::of(1080));
    expectThrows<std::invalid_argument>(([&]() { (void)(hardcover.costFor(0)); }));

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    assert(e.isDigital());
    assert(e.costFor(5) == Money::of(150));
    assert(e.costFor(50) == Money::of(600));
    assert(e.costFor(51) == Money::of(605));
    assert(e.costFor(60) == Money::of(650));

    assert(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    assert(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    assert(os.str().find("platform fee: 100.00") != std::string::npos);

    expectThrows<std::invalid_argument>(([&]() { (void)(Book("", "T", {}, "", "", 2000, Money::of(1))); }));
    expectThrows<std::invalid_argument>(([&]() { (void)(Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1))); }));
}


static void testJournal() {
    Journal j("J1", "Journal of Computing", "Academic Press", 2025,
              Money::of(120), "2049-3630", 12, 3);
    assert(j.category() == ResourceCategory::Journal);
    assert(!j.isDigital());
    assert(j.issn() == "2049-3630");
    assert(j.issuesPerYear() == 12);
    assert(j.subscriptionYears() == 3);
    assert(j.costFor(2) == Money::of(720));
    assert(j.costFor(10) == Money::of(3240));
    expectThrows<std::invalid_argument>(([&]() { (void)(j.costFor(0)); }));
    expectThrows<std::invalid_argument>(([&]() { (void)(Journal("J2", "Bad Journal", "P", 2025, Money::of(10),
                         "issn", 4, 0)); }));

    std::ostringstream os;
    os << j;
    assert(os.str().find("issn: 2049-3630") != std::string::npos);
    assert(os.str().find("issues per year: 12") != std::string::npos);
    assert(os.str().find("subscription years: 3") != std::string::npos);

    Budget budget(Money::of(1000));
    budget.setQuota(ResourceCategory::Journal, {5, Money::of(800)});
    budget.commit(ResourceCategory::Journal, 2, j.costFor(2));
    assert(budget.usageFor(ResourceCategory::Journal).units == 2);
    assert(budget.usageFor(ResourceCategory::Journal).spent == Money::of(720));

    std::ostringstream report;
    budget.print(report);
    assert(report.str().find("Journal") != std::string::npos);
}

static void testEBook() {
    EBook ebook("E1", "C++ in Depth", "Digital Press", 2024, Money::of(20),
                "https://ebooks.example/cpp", {"A. Author", "B. Writer"},
                "978-1234567890", EBookFormat::EPUB, true,
                LicenseModel::AnnualSubscription, Money::of(50));
    assert(ebook.category() == ResourceCategory::EBook);
    assert(ebook.isDigital());
    assert(ebook.authors().size() == 2);
    assert(ebook.isbn() == "978-1234567890");
    assert(ebook.format() == EBookFormat::EPUB);
    assert(ebook.drmProtected());
    assert(ebook.costFor(3) == Money::of(110));

    const Resource& resource = ebook;
    std::ostringstream os;
    resource.print(os);
    assert(os.str().find("access url: https://ebooks.example/cpp") != std::string::npos);
    assert(os.str().find("platform fee: 50.00") != std::string::npos);
    assert(os.str().find("authors: A. Author and B. Writer") != std::string::npos);
    assert(os.str().find("file format: EPUB") != std::string::npos);
    assert(os.str().find("drm protected: yes") != std::string::npos);

    Budget budget(Money::of(500));
    budget.setQuota(ResourceCategory::EBook, {4, Money::of(200)});
    budget.commit(ebook.category(), 2, ebook.costFor(2));
    assert(budget.usageFor(ResourceCategory::EBook).units == 2);
    assert(budget.usageFor(ResourceCategory::ElectronicResource).units == 0);
    assert(std::string(eBookFormatName(EBookFormat::PDF)) == "PDF");
    assert(std::string(eBookFormatName(EBookFormat::HTML)) == "HTML");
}

static void testAudioBookAndThesis() {
    AudioBook audio("A1", "The C++ Journey", {"A. Writer"}, "isbn-a", "Audio Press",
                    2025, Money::of(15), "N. Narrator", 360);
    assert(audio.category() == ResourceCategory::AudioBook);
    assert(audio.authors().size() == 1);
    assert(audio.narrator() == "N. Narrator");
    assert(audio.durationMinutes() == 360);
    assert(audio.costFor(2) == Money::of(30));
    assert(audio.costFor(10) == Money::of(150));
    std::ostringstream audioOut;
    audio.print(audioOut);
    assert(audioOut.str().find("narrator: N. Narrator") != std::string::npos);
    assert(audioOut.str().find("duration minutes: 360") != std::string::npos);
    expectThrows<std::invalid_argument>(([&]() { (void)(AudioBook("A2", "Bad Audio", {}, "i", "P", 2025, Money::of(1),
                           "Narrator", 0)); }));

    Thesis thesis("T1", "Cache Coherence Study", "IIIT Delhi", 2026,
                  "IIIT Delhi", "M.Tech", "Dr. Guide");
    assert(thesis.category() == ResourceCategory::Thesis);
    assert(thesis.unitPrice() == Money{});
    assert(thesis.costFor(3) == Money{});
    assert(thesis.university() == "IIIT Delhi");
    assert(thesis.degree() == "M.Tech");
    assert(thesis.supervisor() == "Dr. Guide");
    std::ostringstream thesisOut;
    thesis.print(thesisOut);
    assert(thesisOut.str().find("university: IIIT Delhi") != std::string::npos);
    assert(thesisOut.str().find("supervisor: Dr. Guide") != std::string::npos);

    Budget budget(Money::of(100));
    budget.setQuota(ResourceCategory::AudioBook, {4, Money::of(80)});
    budget.setQuota(ResourceCategory::Thesis, {3, Money::of(0)});
    budget.commit(audio.category(), 2, audio.costFor(2));
    budget.commit(thesis.category(), 1, thesis.costFor(1));
    assert(budget.usageFor(ResourceCategory::AudioBook).units == 2);
    assert(budget.usageFor(ResourceCategory::Thesis).units == 1);
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    assert(c.size() == 3);
    assert(c.contains("B1"));
    assert(c.find("nope") == nullptr);
    expectThrows<NotFoundError>(([&]() { (void)(c.get("nope")); }));
    expectThrows<DuplicateIdError>(([&]() { (void)(c.emplace<Book>("B1", "dup", std::vector<std::string>{}, "", "", 1,
                                 Money::of(1))); }));

    assert(c.searchTitle("clean").size() == 2);
    assert(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    assert(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    assert(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    assert(c.holdings("B1") == 3);
    expectThrows<std::invalid_argument>(([&]() { (void)(c.addHoldings("B1", -5)); }));

    c.remove("R1");
    assert(c.size() == 2);
    expectThrows<NotFoundError>(([&]() { (void)(c.remove("R1")); }));
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    assert(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
    assert(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
    assert(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
    assert(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
    assert(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    assert(b.spent() == Money::of(300));
    assert(*b.unitsRemaining(ResourceCategory::Book) == 1);
    assert(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    assert(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    expectThrows<QuotaExceededError>(([&]() { (void)(b.commit(ResourceCategory::Book, 2, Money::of(10))); }));
    expectThrows<BudgetExceededError>(([&]() { (void)(b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800))); }));
    expectThrows<std::invalid_argument>(([&]() { (void)(b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1))); }));
    assert(b.spent() == Money::of(300));  // failed commits changed nothing
}

static void testAcquisition() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "i", "P", 2020,
                    Money::of(100));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::AnnualSubscription, Money::of(50));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {3, Money::of(1000)});
    AcquisitionManager acq(c, b);

    assert(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    assert(acq.canPurchase("B1", 3, &why) && why.empty());
    assert(!acq.canPurchase("B1", 4, &why) && !why.empty());
    assert(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    assert(rec.approved && rec.cost == Money::of(200) && rec.orderNo == 1);
    assert(c.holdings("B1") == 2);

    expectThrows<QuotaExceededError>(([&]() { (void)(acq.purchase("B1", 2)); }));
    expectThrows<NotFoundError>(([&]() { (void)(acq.purchase("nope", 1)); }));
    assert(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    assert(res.size() == 5);
    assert(res[0].approved && res[0].cost == Money::of(150));
    assert(!res[1].approved);  // 1050 > remaining 150
    assert(res[2].approved);
    assert(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    assert(!res[4].approved);
    assert(acq.totalSpent() == Money::of(450));
    assert(b.spent() == acq.totalSpent());
    assert(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    assert(acq.history().size() == 6);
}



static void testTitleQuota() {
    Catalog c;
    c.emplace<Book>("TQ1", "First title", std::vector<std::string>{"A"}, "i1", "P",
                    2026, Money::of(20));
    c.emplace<Book>("TQ2", "Second title", std::vector<std::string>{"B"}, "i2", "P",
                    2026, Money::of(20));
    c.emplace<Book>("TQ3", "Third title", std::vector<std::string>{"C"}, "i3", "P",
                    2026, Money::of(20));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {10, Money::of(300), 2});
    AcquisitionManager acq(c, b);

    assert(acq.canPurchase("TQ1", 2));
    assert(acq.purchase("TQ1", 2).approved);
    assert(acq.purchase("TQ1", 1).approved);  // same title does not use another title slot
    assert(b.usageFor(ResourceCategory::Book).titles == 1);
    assert(acq.canPurchase("TQ2", 1));
    assert(acq.purchase("TQ2", 1).approved);
    assert(b.usageFor(ResourceCategory::Book).titles == 2);
    std::string reason;
    assert(!acq.canPurchase("TQ3", 1, &reason));
    assert(reason.find("title quota exceeded") != std::string::npos);
    expectThrows<QuotaExceededError>(([&]() { (void)(acq.purchase("TQ3", 1)); }));
    assert(b.spent() == Money::of(80));
    assert(c.holdings("TQ3") == 0);
    assert(*b.titlesRemaining(ResourceCategory::Book) == 0);
    expectThrows<std::invalid_argument>(([&]() { (void)(b.setQuota(ResourceCategory::Book, {10, Money::of(100), -1})); }));
    assert(!b.check(ResourceCategory::Book, 1, Money::of(1)).empty());
}

static void testTaxes() {
    Catalog c;
    c.emplace<Book>("BT", "Taxable book", std::vector<std::string>{"A"}, "i", "P",
                    2026, Money::of(100));
    c.emplace<ElectronicResource>("ET", "Taxable database", "P", 2026, Money::of(100),
                                  "url", LicenseModel::Perpetual);
    Budget b(Money::of(250));
    b.setQuota(ResourceCategory::Book, {5, Money::of(110)});
    b.setQuota(ResourceCategory::ElectronicResource, {5, Money::of(120)});
    AcquisitionManager acq(c, b);
    acq.setTaxRates(10.0, 20.0);

    assert(acq.quoteBeforeTax("BT", 1) == Money::of(100));
    assert(acq.quote("BT", 1) == Money::of(110));
    assert(acq.quote("ET", 1) == Money::of(120));
    assert(acq.canPurchase("BT", 1));
    assert(acq.canPurchase("ET", 1));
    const auto& bookOrder = acq.purchase("BT", 1);
    assert(bookOrder.preTaxCost == Money::of(100));
    assert(bookOrder.tax == Money::of(10));
    assert(bookOrder.cost == Money::of(110));
    const auto& digitalOrder = acq.purchase("ET", 1);
    assert(digitalOrder.preTaxCost == Money::of(100));
    assert(digitalOrder.tax == Money::of(20));
    assert(digitalOrder.cost == Money::of(120));
    assert(b.spent() == Money::of(230));
    assert(acq.totalBeforeTax() == Money::of(200));
    assert(acq.totalTax() == Money::of(30));
    assert(acq.totalSpent() == Money::of(230));

    std::ostringstream report;
    acq.printReport(report);
    assert(report.str().find("Total before tax: 200.00") != std::string::npos);
    assert(report.str().find("Total tax: 30.00") != std::string::npos);
    assert(report.str().find("Total after tax: 230.00") != std::string::npos);

    Budget tightBudget(Money::of(500));
    tightBudget.setQuota(ResourceCategory::Book, {5, Money::of(105)});
    AcquisitionManager tight(c, tightBudget);
    tight.setTaxRates(10.0, 20.0);
    std::string reason;
    assert(!tight.canPurchase("BT", 1, &reason));
    assert(reason.find("spend quota exceeded") != std::string::npos);
    expectThrows<std::invalid_argument>(([&]() { (void)(tight.setTaxRates(-1.0, 0.0)); }));
}


static void testCancellationDepartmentsAndRollover() {
    Catalog c;
    c.emplace<Book>("C1", "Cancel book", std::vector<std::string>{"Ada"}, "ISBN-C1",
                   "Press", 2020, Money::of(100));
    Budget general(Money::of(1000));
    general.setQuota(ResourceCategory::Book, {10, Money::of(900), 2});
    Budget cs(Money::of(500));
    cs.setQuota(ResourceCategory::Book, {4, Money::of(400), 2});
    AcquisitionManager acq(c, general);
    acq.registerDepartment("Computer Science", cs);
    const auto& order = acq.purchase("C1", 2, "Computer Science");
    const int orderNo = order.orderNo;
    assert(order.department == "Computer Science");
    assert(cs.spent() == Money::of(200));
    assert(general.spent() == Money::of(0));
    assert(c.holdings("C1") == 2);
    const auto& cancellation = acq.cancel(orderNo);
    assert(cancellation.isCancellation && cancellation.referenceOrderNo == orderNo);
    assert(c.holdings("C1") == 0);
    assert(cs.spent() == Money::of(0));
    assert(cs.usageFor(ResourceCategory::Book).units == 0);
    assert(cs.usageFor(ResourceCategory::Book).titles == 0);
    assert(acq.history().size() == 2);
    assert(acq.totalSpent() == Money::of(0));
    expectThrows<std::invalid_argument>([&] { acq.cancel(orderNo); });

    Budget current(Money::of(1000));
    current.setQuota(ResourceCategory::Book, {8, Money::of(500)});
    current.commit(ResourceCategory::Book, 2, Money::of(250), "x");
    Budget next = current.createNextYear(Money::of(200), 50.0);
    assert(next.total() == Money::of(575));
    assert(next.spent() == Money::of(0));
    assert(next.quotaFor(ResourceCategory::Book).has_value());
    expectThrows<std::invalid_argument>([&] { (void)current.createNextYear(Money::of(1), 101.0); });
}

static void testAtomicBatchAndVendors() {
    Catalog c;
    c.emplace<Book>("V1", "Vendor book", std::vector<std::string>{"Grace"}, "V-ISBN",
                    "Press", 2021, Money::of(100));
    c.addVendor("V1", "Vendor A", Money::of(90));
    c.addVendor("V1", "Vendor B", Money::of(75));
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(500)});
    AcquisitionManager acq(c, b);
    assert(acq.quoteBeforeTax("V1", 2) == Money::of(150));
    const auto& bought = acq.purchase("V1", 2);
    assert(bought.vendor == "Vendor B");
    assert(bought.cost == Money::of(150));

    auto aborted = acq.processBatch({{"V1", 1}, {"V1", 10}}, true);
    assert(aborted.size() == 2);
    assert(!aborted[0].approved && !aborted[1].approved);
    assert(c.holdings("V1") == 2);
    assert(b.spent() == Money::of(150));

    auto committed = acq.processBatch({{"V1", 1}, {"V1", 1}}, true);
    assert(committed.size() == 2 && committed[0].approved && committed[1].approved);
    assert(c.holdings("V1") == 4);
}

static void testSearchAndLending() {
    Catalog c;
    c.emplace<Book>("L1", "Algorithms", std::vector<std::string>{"Ada Lovelace"}, "ISBN-L1",
                    "Press", 2018, Money::of(10));
    c.emplace<Journal>("L2", "Physics Journal", "P", 2022, Money::of(5), "ISSN-L2", 4);
    c.emplace<EBook>("L3", "Digital Algorithms", "P", 2024, Money::of(2), "url",
                     std::vector<std::string>{"Ada Lovelace"}, "ISBN-L3", EBookFormat::PDF,
                     false, LicenseModel::Perpetual, Money::fromMinor(0));
    assert(c.searchAuthor("ada").size() == 2);
    assert(c.searchIdentifier("issn-l2").size() == 1);
    assert(c.searchIdentifier("ISBN-L3").size() == 1);
    assert(c.searchYears(2020, 2025).size() == 2);
    expectThrows<std::invalid_argument>([&] { (void)c.searchYears(2025, 2020); });

    c.addHoldings("L1", 2);
    c.borrow("Patron A", "L1");
    c.borrow("Patron B", "L1");
    assert(c.borrowed("Patron A", "L1") == 1);
    expectThrows<std::invalid_argument>([&] { c.borrow("Patron C", "L1"); });
    c.returnCopy("Patron A", "L1");
    assert(c.borrowed("Patron A", "L1") == 0);
    expectThrows<std::invalid_argument>([&] { c.returnCopy("Patron A", "L1"); });

    c.addHoldings("L3", 1);
    c.openSession("Patron A", "L3");
    assert(c.openSessions("Patron A", "L3") == 1);
    expectThrows<std::invalid_argument>([&] { c.openSession("Patron B", "L3"); });
    c.closeSession("Patron A", "L3");
    assert(c.openSessions("Patron A", "L3") == 0);
}

static void testCurrency() {
    Money usd = Money::of(10, 0, "USD");
    Money inr = Money::of(10, 0, "INR");
    assert(usd.currencyCode() == "USD");
    assert(inr.currencyCode() == "INR");
    expectThrows<std::invalid_argument>([&] { (void)(usd + inr); });
    expectThrows<std::invalid_argument>([&] { (void)(usd - inr); });
    expectThrows<std::invalid_argument>([&] { (void)(usd < inr); });
    expectThrows<std::invalid_argument>([&] { (void)(usd == inr); });
    Budget rupeeBudget(Money::of(1000, 0, "INR"));
    rupeeBudget.setQuota(ResourceCategory::Book, {5, Money::of(500, 0, "INR")});
    Catalog c;
    c.emplace<Book>("IN1", "Rupee book", std::vector<std::string>{"A"}, "IN-ISBN", "P",
                    2025, Money::of(100, 0, "INR"));
    AcquisitionManager acq(c, rupeeBudget);
    assert(acq.quote("IN1", 2) == Money::of(200, 0, "INR"));
    acq.purchase("IN1", 2);
    assert(rupeeBudget.spent() == Money::of(200, 0, "INR"));
}

int main() {
    testMoney();
    testResourcesAndCost();
    testJournal();
    testEBook();
    testAudioBookAndThesis();
    testCatalog();
    testBudget();
    testAcquisition();
    testTitleQuota();
    testTaxes();
    testCancellationDepartmentsAndRollover();
    testAtomicBatchAndVendors();
    testSearchAndLending();
    testCurrency();
    std::cout << "All assertion-based tests passed\n";
    return 0;
}
